// HTTPS for iOS through NSURLSession: the system TLS stack and certificate
// store, no libcurl (the iOS SDK ships none). Same contract as posix_http.cpp:
// HTTPS only (redirects included), bounded responses, streamed downloads with
// progress and cancellation. The shader pack download and update check use it.
#include "http.h"
#include "progress.h"

#import <Foundation/Foundation.h>

#include <functional>
#include <fstream>
#include <string_view>

namespace
{
constexpr NSTimeInterval RequestTimeout = 8.0;
constexpr NSTimeInterval StallTimeout = 30.0;

bool IsHttpsUrl(std::string_view url)
{
    constexpr std::string_view prefix = "https://";
    return url.size() >= prefix.size() && url.substr(0, prefix.size()) == prefix;
}

NSURL* MakeUrl(std::string_view url)
{
    NSString* string = [[NSString alloc] initWithBytes:url.data() length:url.size() encoding:NSUTF8StringEncoding];
    return string ? [NSURL URLWithString:string] : nil;
}
}

// Streams one transfer into a sink; the calling thread waits on the semaphore.
@interface LoHttpTransfer : NSObject <NSURLSessionDataDelegate>
{
@public
    std::function<bool(const void*, size_t)> sink; // false stops the transfer
    bool stopped;
}
@property(nonatomic) dispatch_semaphore_t finished;
@property(nonatomic) NSInteger status;
@property(nonatomic) BOOL insecureRedirect;
@property(nonatomic, copy) NSString* failure;
@end

@implementation LoHttpTransfer

- (instancetype)init
{
    if ((self = [super init]))
    {
        _finished = dispatch_semaphore_create(0);
        stopped = false;
    }
    return self;
}

- (void)URLSession:(NSURLSession*)session
                          task:(NSURLSessionTask*)task
    willPerformHTTPRedirection:(NSHTTPURLResponse*)response
                    newRequest:(NSURLRequest*)request
             completionHandler:(void (^)(NSURLRequest*))completionHandler
{
    if (![request.URL.scheme isEqualToString:@"https"])
    {
        self.insecureRedirect = YES;
        completionHandler(nil);
        return;
    }
    completionHandler(request);
}

- (void)URLSession:(NSURLSession*)session
              dataTask:(NSURLSessionDataTask*)dataTask
    didReceiveResponse:(NSURLResponse*)response
     completionHandler:(void (^)(NSURLSessionResponseDisposition))completionHandler
{
    if ([response isKindOfClass:[NSHTTPURLResponse class]])
        self.status = ((NSHTTPURLResponse*)response).statusCode;
    if (![response.URL.scheme isEqualToString:@"https"])
        self.insecureRedirect = YES;
    completionHandler(self.status == 200 && !self.insecureRedirect ? NSURLSessionResponseAllow
                                                                    : NSURLSessionResponseCancel);
}

- (void)URLSession:(NSURLSession*)session dataTask:(NSURLSessionDataTask*)dataTask didReceiveData:(NSData*)data
{
    if (stopped) return;
    [data enumerateByteRangesUsingBlock:^(const void* bytes, NSRange range, BOOL* stop) {
        if (!sink(bytes, range.length))
        {
            stopped = true;
            *stop = YES;
        }
    }];
    if (stopped) [dataTask cancel];
}

- (void)URLSession:(NSURLSession*)session task:(NSURLSessionTask*)task didCompleteWithError:(NSError*)error
{
    if (error && !stopped && self.status == 200 && !self.insecureRedirect)
        self.failure = error.localizedDescription ?: @"request failed";
    dispatch_semaphore_signal(self.finished);
}
@end

namespace updater
{
namespace
{
// Runs one GET to completion. Returns false with error set on any failure;
// stopped reports that the sink ended the transfer.
bool Transfer(std::string_view url, NSTimeInterval timeout, NSTimeInterval resourceTimeout,
              const std::function<bool(const void*, size_t)>& sink, std::string& error, bool& stopped)
{
    stopped = false;
    if (!IsHttpsUrl(url))
    {
        error = "update URL does not use HTTPS";
        return false;
    }
    @autoreleasepool
    {
        NSURL* target = MakeUrl(url);
        if (!target)
        {
            error = "invalid update URL";
            return false;
        }
        NSURLSessionConfiguration* configuration = [NSURLSessionConfiguration ephemeralSessionConfiguration];
        configuration.timeoutIntervalForRequest = timeout;
        if (resourceTimeout > 0) configuration.timeoutIntervalForResource = resourceTimeout;
        configuration.HTTPAdditionalHeaders = @{@"User-Agent" : @"LostOdysseyRecomp-Updater/1.0"};
        LoHttpTransfer* transfer = [[LoHttpTransfer alloc] init];
        transfer->sink = sink;
        NSOperationQueue* queue = [[NSOperationQueue alloc] init];
        queue.maxConcurrentOperationCount = 1;
        NSURLSession* session = [NSURLSession sessionWithConfiguration:configuration delegate:transfer delegateQueue:queue];
        [[session dataTaskWithURL:target] resume];
        dispatch_semaphore_wait(transfer.finished, DISPATCH_TIME_FOREVER);
        [session finishTasksAndInvalidate];
        stopped = transfer->stopped;
        if (stopped) return false;
        if (transfer.insecureRedirect)
        {
            error = "update redirect resolved to non-HTTPS URL";
            return false;
        }
        if (transfer.failure)
        {
            error = std::string("request failed: ") + transfer.failure.UTF8String;
            return false;
        }
        if (transfer.status != 200)
        {
            error = "update server returned HTTP " + std::to_string(long(transfer.status));
            return false;
        }
        return true;
    }
}
}

bool ReadResponse(std::string_view url, size_t limit, std::string& body, std::string& error)
{
    body.clear();
    bool exceeded = false, stopped = false;
    const bool ok = Transfer(url, RequestTimeout, RequestTimeout, [&](const void* data, size_t size) {
        if (size > limit - body.size())
        {
            exceeded = true;
            return false;
        }
        body.append(static_cast<const char*>(data), size);
        return true;
    }, error, stopped);
    if (exceeded)
    {
        error = "update response exceeded its size limit";
        return false;
    }
    return ok;
}

bool DownloadFile(std::string_view url, const std::filesystem::path& destination, uint64_t expectedSize,
                  const DownloadProgress& progress, std::string& error, bool& cancelled)
{
    cancelled = false;
    if (!IsHttpsUrl(url))
    {
        error = "update URL does not use HTTPS";
        return false;
    }
    std::ofstream output(destination, std::ios::binary | std::ios::trunc);
    if (!output)
    {
        error = "could not create update download";
        return false;
    }
    uint64_t total = 0;
    bool writeFailed = false, userCancelled = false, stopped = false;
    if (!progress(0, expectedSize))
    {
        cancelled = true;
        error = "update cancelled by user";
        return false;
    }
    // No overall limit for large files, but a stalled transfer fails.
    const bool ok = Transfer(url, StallTimeout, 0, [&](const void* data, size_t size) {
        output.write(static_cast<const char*>(data), std::streamsize(size));
        if (!output)
        {
            writeFailed = true;
            return false;
        }
        total += size;
        if (!progress(total, expectedSize))
        {
            userCancelled = true;
            return false;
        }
        return true;
    }, error, stopped);
    if (userCancelled)
    {
        cancelled = true;
        error = "update cancelled by user";
        return false;
    }
    if (writeFailed)
    {
        error = "could not write update download";
        return false;
    }
    if (!ok) return false;
    output.flush();
    if (!output)
    {
        error = "could not write update download";
        return false;
    }
    return true;
}

bool Download(std::string_view url, const std::filesystem::path& destination, uint64_t expectedSize,
              ProgressWindow& progress, std::string& error, bool& cancelled)
{
    return DownloadFile(url, destination, expectedSize, [&progress](uint64_t completed, uint64_t total) {
        if (completed) progress.SetDownloadProgress(completed, total);
        return !progress.Cancelled();
    }, error, cancelled);
}
}
