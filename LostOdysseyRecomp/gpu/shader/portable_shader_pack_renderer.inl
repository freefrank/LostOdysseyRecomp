// Included inside the renderer implementation class. All methods run on the
// command processor thread; existing pipeline workers see immutable shader maps.
std::unique_ptr<xenos::portable_pack::Reader> portableShaderPack;
std::unique_ptr<xenos::portable_pack::Writer> portableShaderExport;

bool PortableExportRequested() const
{
    const char* path = std::getenv("LO_SHADER_EXPORT_PACK");
    return path && *path;
}

xenos::portable_pack::PackFormat PortablePackFormat() const
{
    return vulkan ? xenos::portable_pack::PackFormat::Spirv : xenos::portable_pack::PackFormat::Dxil;
}

xenos::portable_pack::Digest PortableShaderContract(std::span<const uint8_t> xex) const
{
    const auto format = PortablePackFormat();
    return xenos::portable_pack::Contract(cacheIdentity.translatorVersion, cacheIdentity.options,
        cacheIdentity.variant, xenos::kShaderCommonHlsl,
        xenos::resources::variants::DiscoveryIdentity, xex, 1, format);
}

bool TryOpenPortableShaderPack(std::span<const uint8_t> xex)
{
    if ((cacheIdentity.backend != xenos::cache::Backend::Vulkan &&
         cacheIdentity.backend != xenos::cache::Backend::D3D12) ||
        PortableExportRequested() || std::getenv("LO_NO_PORTABLE_SHADER_PACK") ||
        std::getenv("LO_SHADER_FULL_SCAN") || std::getenv("LO_SHADER_HLSL_DIR") ||
        std::getenv("LO_SHADER_RETRY_FAILURES")) return false;
    try {
        const char* configured = std::getenv("LO_SHADER_PACK_PATH");
        const auto path = configured && *configured ? std::filesystem::path(configured) :
            xenos::portable_pack::DefaultPath(PortablePackFormat());
        std::error_code ec;
        if (!std::filesystem::is_regular_file(path, ec)) {
            if (configured && *configured) LOG_WARNING("renderer: configured portable shader pack is missing: {}", path.string());
            return false;
        }
        auto pack = std::make_unique<xenos::portable_pack::Reader>(path,
            PortableShaderContract(xex), PortablePackFormat());
        const auto& report = pack->Info();
        LOG_INFO("renderer: portable shader pack hit: {} records, {} unique binaries, {} file bytes, {} index bytes; lazy modules, no guest shader DXC prebuild",
            report.records, report.uniqueBinaries, report.fileBytes, report.indexBytes);
        portableShaderPack = std::move(pack);
        return true;
    } catch (const std::exception& e) {
        LOG_WARNING("renderer: portable shader pack rejected; local cache fallback: {}", e.what());
        portableShaderPack.reset();
        return false;
    }
}

bool TryLoadPortableShader(bool pixel, uint64_t hash)
{
    if (!portableShaderPack || !hash) return false;
    auto& cache = shaders[pixel ? 1 : 0];
    if (const auto found = cache.find(hash); found != cache.end()) return found->second.valid;
    try {
        auto record = portableShaderPack->Get(pixel, hash);
        if (!record) return false; // Missing is not a negative-cache entry.
        auto module = device->createShader(record->binary.data(), record->binary.size(), "main", renderFormat);
        if (!module) throw std::runtime_error("portable shader module creation failed");
        // Publish only after both decompression/validation and module creation.
        auto& entry = cache[hash];
        entry.info = std::move(record->info);
        entry.shader = std::move(module);
        entry.valid = true;
        return true;
    } catch (const std::exception& e) {
        LOG_WARNING("renderer: portable shader {}_{:016x}: {}; disabling pack and retrying through local shader path",
            pixel ? "ps" : "vs", hash, e.what());
        portableShaderPack.reset();
        return false;
    }
}

void BeginPortableShaderExport(std::span<const uint8_t> xex)
{
    portableShaderExport.reset();
    if (!PortableExportRequested()) return;
    try {
        if (cacheIdentity.backend != xenos::cache::Backend::Vulkan &&
            cacheIdentity.backend != xenos::cache::Backend::D3D12)
            throw std::runtime_error("portable export requires Vulkan or D3D12");
        const auto& producer = xenos::DxcIdentity();
        if (producer.empty()) throw std::runtime_error("cannot certify export without compiler identity");
        const auto path = std::filesystem::path(std::getenv("LO_SHADER_EXPORT_PACK"));
        const auto format = PortablePackFormat();
        if (path.extension() != (format == xenos::portable_pack::PackFormat::Spirv ? ".lospv" : ".lospd"))
            throw std::runtime_error("portable export path extension does not match renderer backend");
        portableShaderExport = std::make_unique<xenos::portable_pack::Writer>(path,
            PortableShaderContract(xex), producer, format);
    } catch (const std::exception& e) {
        LOG_WARNING("renderer: portable shader export unavailable: {}", e.what());
    }
}

void ExportPortableShader(uint64_t hash, const xenos::TranslatedShader& info,
    std::span<const uint8_t> binary, std::string_view failure, bool deterministic)
{
    if (!portableShaderExport) return;
    try {
        if (failure.empty()) portableShaderExport->Add(hash, info, binary);
        else if (deterministic)
            portableShaderExport->OmitFailure(info.hlsl.size(), info.errors.size() + failure.size());
        else throw std::runtime_error("transient compiler failure; incomplete export discarded");
    } catch (const std::exception& e) {
        LOG_WARNING("renderer: portable shader export abandoned: {}", e.what());
        portableShaderExport.reset();
    }
}

void FinishPortableShaderExport()
{
    if (!portableShaderExport) return;
    try {
        const auto r = portableShaderExport->Finish();
        LOG_INFO("renderer: portable shader pack published: {} records, {} unique binaries, {} deterministic failures omitted; {} binary bytes -> {} unique bytes -> {} compressed bytes; {} reconstructed HLSL bytes and {} diagnostic bytes omitted; final file {} bytes",
            r.records, r.uniqueBinaries, r.failuresOmitted, r.binaryBytes, r.uniqueBinaryBytes,
            r.compressedBytes, r.hlslBytesOmitted, r.diagnosticBytesOmitted, r.fileBytes);
    } catch (const std::exception& e) {
        LOG_WARNING("renderer: portable shader export not published: {}", e.what());
    }
    portableShaderExport.reset();
}
