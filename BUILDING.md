# Windows 빌드

**프로그램을 사용하려는 경우에는 빌드가 필요 없습니다. [README의 실행용 다운로드와 설치 순서](README.md)를 따라가세요.** 이 문서는 소스를 수정해 직접 EXE를 만드는 개발용 안내입니다.

## 확인에 사용한 환경

- x64 Windows
- MSVC 14.44.35207 (Visual Studio 2022 C++ 도구)
- Windows SDK 10.0.26100.0
- Qt 6.12.0 msvc2022_64: qtbase, qtdeclarative, qtsvg, qttranslations, qttools
- Moonlight Windows 의존 라이브러리 v20
- Qt jom 또는 nmake

Qt·컴파일러·SDK·jom은 별도 설치합니다. 로컬 도구 경로나 바이너리는 포함하지 않았습니다.

## 클라이언트

1. MSVC x64 Native Tools 명령 프롬프트를 열고 저장소 폴더로 이동합니다.
2. Qt 6.12.0 msvc2022_64의 bin과 jom 디렉터리를 PATH에 추가합니다.
3. PowerShell에서 setup-deps.ps1을 실행해 upstream v20 빌드 라이브러리를 받습니다. 이 스크립트는 libs/windows 내용을 다시 구성합니다.
4. 같은 개발 환경에서 아래 명령을 실행합니다.

    scripts\build-pen-windows.cmd

필압·단축키 검사를 먼저 실행하고 qmake 및 Release 빌드를 수행합니다. 출력은 build-public/app/release/Moonlight.exe입니다.

현재 저장소에는 의존 소스가 포함되어 별도 서브모듈 초기화가 필요하지 않습니다. 다른 플랫폼과 원본 빌드 안내는 README.upstream.md를 참고하세요.

## 실행 패키지 만들기

개인 설정이 없는 Qt 배포 런타임을 별도 폴더에 준비한 뒤 `scripts/package-windows-release.py --runtime <런타임> --output <새 출력 폴더> --tag <대응 소스 태그>`를 실행합니다. 사용 중인 `app` 폴더를 그대로 포장하지 마세요.

클립보드를 포함하는 현재 배포판은 같은 명령에 `--clipboard-host <검증한 r4 호스트 패키지 폴더>`를 추가합니다. 호스트 폴더에는 `host-clipboard/package`의 설치기·검증기와 대응 Sunshine 실행 파일·`package.json`·라이선스·패치가 필요합니다. 패키지 도구는 r4 리비전·16MiB 한도·호스트 실행 파일 해시를 확인하고, 새 클라이언트 해시를 호스트 매니페스트에 기록합니다.

01 문라이트, 02 통합 호스트 도구와 기존 화면 전환 전용 ZIP을 만듭니다. 02 통합 도구의 `호스트 설치.cmd`는 클립보드 설치 성공 후 화면 전환 바로가기를 등록합니다. `삭제.cmd`는 공식 Sunshine 복원 성공 후 바로가기를 제거합니다. 실제 호스트를 변경하지 않는 사전 검사는 출력 폴더의 `support/host-tools.ps1 -CheckOnly`로 실행합니다. 사전 검사 성공과 실제 두 PC의 복사·붙여넣기 성공은 별도로 확인합니다.

## 호스트 도우미

권장 설치는 Releases의 02 호스트 ZIP이며, 소스에서 직접 실행하려면 host-shortcuts/INSTALL-HOST-SHORTCUTS.cmd를 사용합니다. Sunshine 호스트에서 실행하면 시작 메뉴 바로가기에 D/E/H 조합을 저장합니다. 복제/확장은 Windows 기본 DisplaySwitch.exe를 실행하고, ? 버튼은 Windows 디스플레이 설정을 엽니다. 스크립트 생성 결과를 검사했고 사용자의 실제 호스트에서 동작을 확인받았습니다. 모든 디스플레이 구성에서의 검증을 뜻하지는 않습니다.

아래는 선택적인 기존 네이티브 도우미 빌드 절차입니다. 해당 EXE가 호스트 보안 프로그램에서 차단된 사례가 있어 차단 해제나 보안 예외 설정을 전제로 배포하지 않습니다.

같은 MSVC x64 개발 환경에서:

    scripts\build-host-helper.cmd

build-public/host-helper/Moonlight-Display-Helper.exe를 만들고 --self-test를 실행합니다. 자체 검사는 실제 화면 구성을 바꾸지 않습니다.

설치 패키지는 해당 EXE와 host-helper/package의 파일을 같은 폴더에 담아 만듭니다. Sunshine 호스트에서 INSTALL-HOST.cmd를 실행하면 현재 사용자에게 설치하고 로그인 시 자동 실행을 등록합니다.

기존 Moonlight/Sunshine 입력 경로에서 Ctrl+Alt+Shift+D(복제), F14(확장), F15(연결 확인)를 받습니다. SetDisplayConfig로 검사 후 적용하고 QueryDisplayConfig로 결과를 읽습니다. 결과는 호스트 알림과 사용자 LocalAppData의 helper.log에 기록합니다. 별도 네트워크 서버는 만들지 않습니다.

## 공개 전 재검사

Gitleaks를 PATH에 설치한 뒤:

    powershell -NoProfile -File scripts/check-public.ps1

현재 폴더는 소스만 포함합니다. 나중에 추가하는 실행 패키지나 설정 파일은 다시 검사해야 합니다.
