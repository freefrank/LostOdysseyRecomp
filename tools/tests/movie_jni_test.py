#!/usr/bin/env python3
"""Real JNI calls with scripted Android classes; no codec/hardware claims."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

JAVA = {
    "android/os/Build.java": """
package android.os;
public final class Build { public static final class VERSION { public static int SDK_INT = 35; } }
""",
    "android/graphics/Rect.java": """
package android.graphics;
public final class Rect { public int left=2, top=2, right=6, bottom=6; }
""",
    "android/view/Surface.java": "package android.view; public final class Surface {}",
    "android/media/MediaCrypto.java": "package android.media; public final class MediaCrypto {}",
    "android/media/MediaFormat.java": """
package android.media;
import java.nio.ByteBuffer;
import java.util.HashMap;
public final class MediaFormat {
    private final HashMap<String,Object> values = new HashMap<>();
    public static MediaFormat createVideoFormat(String mime, int w, int h) {
        if (!mime.equals("video/avc") || w!=4 || h!=4) throw new AssertionError("format dimensions/mime");
        return new MediaFormat();
    }
    public void setInteger(String key, int value) { values.put(key,value); }
    public void setFloat(String key, float value) { values.put(key,value); }
    public void setByteBuffer(String key, ByteBuffer value) { values.put(key,value); }
    public void validate() {
        if (!values.get("color-format").equals(0x7F420888)) throw new AssertionError("not flexible YUV420");
        for (String key : new String[]{"csd-0","csd-1"}) {
            ByteBuffer b=((ByteBuffer)values.get(key)).duplicate();
            if (b.getInt()!=1 || b.get()!=(byte)(key.equals("csd-0")?0x67:0x68)) throw new AssertionError("CSD/start code");
        }
    }
}
""",
    "android/media/MediaCodecInfo.java": """
package android.media;
public final class MediaCodecInfo {
    private final String name;
    public MediaCodecInfo(String name) { this.name=name; }
    public String getName() { return name; }
    public boolean isEncoder() { return false; }
    public String[] getSupportedTypes() { return new String[]{"video/hevc","video/avc"}; }
    public CodecCapabilities getCapabilitiesForType(String mime) {
        if (!mime.equals("video/avc")) throw new AssertionError("wrong capability mime");
        return new CodecCapabilities();
    }
    public boolean isHardwareAccelerated() { return !name.startsWith("OMX.google."); }
    public boolean isSoftwareOnly() { return name.startsWith("OMX.google."); }
    public static final class CodecCapabilities {
        public boolean isFormatSupported(MediaFormat f) { return true; }
    }
}
""",
    "android/media/MediaCodecList.java": """
package android.media;
public final class MediaCodecList {
    public MediaCodecList(int kind) { if(kind!=0) throw new AssertionError("non-regular codec list"); }
    public MediaCodecInfo[] getCodecInfos() { return new MediaCodecInfo[]{new MediaCodecInfo("OMX.vendor.avc"),new MediaCodecInfo("OMX.google.h264.decoder")}; }
}
""",
    "android/media/Image.java": """
package android.media;
import android.graphics.Rect;
import java.nio.ByteBuffer;
public final class Image implements AutoCloseable {
    private final Plane[] planes;
    private boolean closed;
    public Image(int tag, boolean broken) {
        MediaCodec.liveImages++;
        ByteBuffer y=ByteBuffer.allocateDirect(3+12*5+8);
        y.position(3);
        for(int row=0;row<6;row++) for(int x=0;x<8;x++) y.put(3+row*12+x,(byte)(tag*16+row*8+x));
        // Interleaved VU with separate overlapping views and no final padding.
        ByteBuffer vu=ByteBuffer.allocateDirect(8*2+8);
        for(int row=0;row<3;row++) for(int x=0;x<4;x++) {
            vu.put(row*8+x*2,(byte)(160+row*4+x+tag*16));
            vu.put(row*8+x*2+1,(byte)(80+row*4+x+tag*16));
        }
        ByteBuffer u=vu.duplicate(); u.position(1); u=u.slice();
        ByteBuffer v=vu.duplicate(); v.limit(v.limit()-1);
        if(broken) v.limit(1);
        planes=new Plane[]{new Plane(y,12,1),new Plane(u,8,2),new Plane(v,8,2)};
    }
    public int getFormat() { return 35; }
    public int getWidth() { return 8; }
    public int getHeight() { return 6; }
    public Rect getCropRect() { return new Rect(); }
    public Plane[] getPlanes() { return planes; }
    public void close() { if(closed) throw new AssertionError("image closed twice"); closed=true; MediaCodec.liveImages--; }
    public static final class Plane {
        private final ByteBuffer data;
        private final int row,pixel;
        Plane(ByteBuffer data,int row,int pixel) { this.data=data; this.row=row; this.pixel=pixel; }
        public ByteBuffer getBuffer() { return data; }
        public int getRowStride() { return row; }
        public int getPixelStride() { return pixel; }
    }
}
""",
    "android/media/MediaCodec.java": """
package android.media;
import android.view.Surface;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Collections;
public final class MediaCodec {
    public static String scenario="normal";
    public static int liveImages,liveCodecs,hardwareCreated,softwareCreated;
    private final boolean hardware;
    private final ByteBuffer input=ByteBuffer.allocateDirect(64);
    private final ArrayList<Long> timestamps=new ArrayList<>();
    private boolean running,released,formatSent,eos;
    private int index,outstanding=-1;
    private MediaCodec(String name) { hardware=!name.startsWith("OMX.google."); liveCodecs++; if(hardware) hardwareCreated++; else softwareCreated++; }
    public static void reset(String mode) { scenario=mode; liveImages=liveCodecs=hardwareCreated=softwareCreated=0; }
    public static MediaCodec createByCodecName(String name) { return new MediaCodec(name); }
    public void configure(MediaFormat f,Surface surface,MediaCrypto crypto,int flags) {
        if(surface!=null || crypto!=null || flags!=0) throw new AssertionError("non-ByteBuffer mode");
        f.validate();
        if(hardware && scenario.equals("configure_error")) throw new IllegalStateException("scripted configure error");
    }
    public void start() { if(running || released) throw new AssertionError("start state"); running=true; }
    public void stop() { if(!running) throw new AssertionError("stop state"); running=false; if(liveImages!=0) throw new AssertionError("image survives stop"); }
    public void release() { if(released || running) throw new AssertionError("release state"); released=true; liveCodecs--; }
    public int dequeueInputBuffer(long timeout) { return !eos && !scenario.equals("cancel_wait") ? 0 : -1; }
    public ByteBuffer getInputBuffer(int index) { return input; }
    public void queueInputBuffer(int index,int offset,int size,long pts,int flags) {
        if(flags==4) { eos=true; Collections.sort(timestamps); return; }
        if(flags!=0 || offset!=0 || size!=6 || input.getInt(0)!=1 || input.get(4)!=(byte)0x65 || pts<0) throw new AssertionError("access unit/PTS/config flags");
        timestamps.add(pts);
    }
    public int dequeueOutputBuffer(BufferInfo info,long timeout) {
        if(!formatSent) { formatSent=true; return -2; }
        if(scenario.equals("cancel_wait")) {
            if(timeout>0) try { Thread.sleep(timeout/1000); } catch(InterruptedException e) { Thread.currentThread().interrupt(); }
            return -1;
        }
        if(hardware && scenario.equals("midstream_error") && index==1) throw new IllegalStateException("scripted midstream error");
        if(!eos) return -1;
        if(outstanding>=0) throw new AssertionError("output not released");
        outstanding=index;
        if((hardware && scenario.equals("empty_eos")) || index==timestamps.size()) { info.size=0; info.flags=4; info.presentationTimeUs=0; return index; }
        info.size=24; info.flags=index+1==timestamps.size()?4:0; info.presentationTimeUs=timestamps.get(index); return index;
    }
    public Image getOutputImage(int index) {
        if(index!=outstanding) throw new AssertionError("wrong output index");
        if(hardware && scenario.equals("no_image")) return null;
        return new Image(index,hardware && scenario.equals("bad_plane"));
    }
    public void releaseOutputBuffer(int index,boolean render) {
        if(index!=outstanding || render || liveImages!=0) throw new AssertionError("wrong output release/ownership");
        outstanding=-1; this.index++;
    }
    public static final class BufferInfo { public int size,flags; public long presentationTimeUs; }
}
""",
    "MovieCodecTest.java": """
import android.media.MediaCodec;
import android.os.Build;
public final class MovieCodecTest {
    private static native void run(String scenario,boolean software);
    public static void main(String[] args) {
        System.loadLibrary("movie_jni_test");
        int cases=0;
        for(String mode:new String[]{"normal","configure_error","no_image","bad_plane","empty_eos","midstream_error","api28","cancel_before_open","cancel_wait","software"}) {
            MediaCodec.reset(mode);
            Build.VERSION.SDK_INT=mode.equals("api28")?28:35;
            run(mode,mode.equals("software"));
            if(MediaCodec.liveImages!=0 || MediaCodec.liveCodecs!=0) throw new AssertionError("JNI image/codec leak: "+mode);
            if(mode.equals("midstream_error") && MediaCodec.softwareCreated!=0) throw new AssertionError("replayed after publishing a frame");
            if(mode.equals("software") && MediaCodec.hardwareCreated!=0) throw new AssertionError("software request created hardware codec");
            if((mode.equals("configure_error") || mode.equals("no_image") || mode.equals("bad_plane") || mode.equals("empty_eos")) && MediaCodec.softwareCreated==0) throw new AssertionError("no software fallback");
            System.out.println("JNI protocol: "+mode+" passed");
            cases++;
        }
        System.out.println(cases+" scripted JNI cases passed; no Android decoder/hardware execution");
    }
}
""",
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--jni-include", action="append", default=[])
    parser.add_argument("--cxx", default=os.environ.get("CXX", "g++"))
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    java = shutil.which("java")
    if not java:
        raise SystemExit("Java runtime/compiler required")
    includes = [Path(p) for p in args.jni_include]
    if not includes:
        jdk = Path(os.environ.get("JAVA_HOME", Path(java).resolve().parent.parent))
        includes = [jdk / "include", jdk / "include" / "linux"]
    if not any((p / "jni.h").is_file() for p in includes):
        raise SystemExit("JDK JNI headers required (or --jni-include)")
    with tempfile.TemporaryDirectory(prefix="lo-movie-jni-") as temp:
        temp = Path(temp)
        sources = []
        for name, content in JAVA.items():
            path = temp / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content)
            sources.append(str(path))
        subprocess.run([java, "--add-modules", "jdk.compiler", "com.sun.tools.javac.Main", "-Xlint:all", "-Werror", "-d", str(temp), *sources], check=True)
        command = [args.cxx, "-std=c++20", "-D__ANDROID__", "-shared", "-fPIC", "-pthread", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                   "-fsanitize=undefined", "-fno-sanitize-recover=undefined", "-I"+str(root / "LostOdysseyRecomp"), "-I"+str(root / "thirdparty/SDL/include")]
        command.extend("-I"+str(p) for p in includes)
        subprocess.run([*command, str(root / "tools/tests/movie_jni_test.cpp"), "-o", str(temp / "libmovie_jni_test.so")], check=True)
        result = subprocess.run([java, "-Xcheck:jni", "-ea", "-Djava.library.path="+str(temp), "-cp", str(temp), "MovieCodecTest"], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        print(result.stdout, end="")
        if result.returncode or "WARNING in native method" in result.stdout or "FATAL ERROR" in result.stdout:
            raise SystemExit(result.returncode or 1)


if __name__ == "__main__":
    main()
