# Windows HEVC 보안 수정 소스

Moonlight Windows 의존 라이브러리 v20을 기준으로 시험판 01에 사용한 FFmpeg 패치와 빌드 조건을 기록합니다. 실행 파일, DLL, 사용자 프로필 및 전체 FFmpeg 소스 아카이브는 포함하지 않습니다.

## 소스와 적용 순서

- FFmpeg 기준 revision: `946fcce07b6dcd0331c8cc609192aeff5e1924f8`
- 기존 Moonlight 호환 패치: `patches/moonlight-v20-hevc-rext.patch`
- Windows HEVC 보안 보완: `patches/windows-hevc-bounds.patch`
- 원본 수정안: 공개된 FFmpeg 개발 메일의 패치 `a4983189482bd8e957c06b45ac49473f8e08cd01` ([출처](https://www.mail-archive.com/ffmpeg-devel@ffmpeg.org/msg191199.html))

기존 Moonlight HEVC RangeExt 형식과 필드를 유지한 채 보안 보완을 적용합니다. 소스 revision, 패치 해시 및 시험에 사용한 소스 아카이브 해시는 [source.json](source.json)에 기록했습니다. 아카이브 해시는 해당 전달 아카이브의 식별값이며, 다른 서버가 다시 생성한 ZIP의 바이트 동일성을 보장하지 않습니다.

MSYS 셸에서 아래 변수들을 실제 개발용 경로로 지정한 뒤 FFmpeg 소스 루트에서 순서대로 적용합니다. `MOONLIGHT_PEN_SOURCE`는 이 저장소, `FFMPEG_SOURCE`는 위 revision의 수정 전 FFmpeg 소스입니다.

```sh
cd "$FFMPEG_SOURCE"
git apply --check "$MOONLIGHT_PEN_SOURCE/ffmpeg/patches/moonlight-v20-hevc-rext.patch"
git apply "$MOONLIGHT_PEN_SOURCE/ffmpeg/patches/moonlight-v20-hevc-rext.patch"
git apply --check "$MOONLIGHT_PEN_SOURCE/ffmpeg/patches/windows-hevc-bounds.patch"
git apply "$MOONLIGHT_PEN_SOURCE/ffmpeg/patches/windows-hevc-bounds.patch"
```

이 적용 순서는 전달 아카이브의 관련 원본 파일로 다시 확인했습니다.

## 시험판의 빌드 조건

전달된 빌드 기록은 x64 MSVC 14.51.36231, Windows SDK 10.0.26100.0 및 MSYS의 make/pkg-config를 사용합니다. MSVC x64 개발 환경을 초기화하고 v20 x64 의존 라이브러리의 헤더, 라이브러리와 dav1d pkg-config 메타데이터를 준비해야 합니다. 개발 도구는 이 저장소에 포함하지 않습니다.

`MOONLIGHT_V20_DEPS`는 준비한 x64 의존 라이브러리 루트, `FFMPEG_PREFIX`는 별도의 빌드 출력 위치입니다. 아래는 개인 경로를 제거한 configure 조건입니다.

```sh
cd "$FFMPEG_SOURCE"
./configure \
 --prefix="$FFMPEG_PREFIX" --arch=x86_64 --toolchain=msvc --extra-ldflags=-cetcompat \
 --enable-pic --enable-lto --fatal-warnings --enable-shared --disable-static --disable-all --disable-autodetect \
 --enable-dxva2 --enable-d3d11va --enable-d3d12va --enable-vulkan \
 --enable-avcodec --enable-avformat --enable-swscale \
 --enable-decoder=h264 --enable-decoder=hevc --enable-decoder=av1 \
 --enable-hwaccel=h264_dxva2 --enable-hwaccel=hevc_dxva2 --enable-hwaccel=av1_dxva2 \
 --enable-hwaccel=h264_d3d11va --enable-hwaccel=hevc_d3d11va --enable-hwaccel=av1_d3d11va \
 --enable-hwaccel=h264_d3d11va2 --enable-hwaccel=hevc_d3d11va2 --enable-hwaccel=av1_d3d11va2 \
 --enable-hwaccel=h264_d3d12va --enable-hwaccel=hevc_d3d12va --enable-hwaccel=av1_d3d12va \
 --enable-libdav1d --enable-decoder=libdav1d \
 --extra-cflags="-I$MOONLIGHT_V20_DEPS/include -I$MOONLIGHT_V20_DEPS/include/x64" \
 --enable-hwaccel=h264_vulkan --enable-hwaccel=hevc_vulkan --enable-hwaccel=av1_vulkan
make -j3
make install
```

기존 `setup-deps.ps1`은 upstream v20의 배포 라이브러리를 받는 도구이며, 이 FFmpeg 패치를 빌드하거나 적용하지 않습니다.

빌드 결과를 평가할 때는 새 시험용 런타임을 사용하고 기존 사용 폴더를 덮어쓰지 않습니다. 이번 소스 공개 작업에서는 위 빌드 절차를 새로 실행하거나, 전체 화면 전환 수정과 보안 DLL을 조합한 새 실행 파일을 만들지 않았습니다. 전달된 시험판의 확인 범위는 [노트북 검증 결과](../docs/validation/security-trial01-20261117.md)를 참고하세요.
