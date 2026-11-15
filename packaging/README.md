# 실행용 ZIP 관리

이 문서는 배포 파일을 다시 만드는 개발용 메모입니다. **설치만 하려면 [메인 안내](../README.md)를 읽으세요.**

## 구성

- 01 노트북 ZIP: 깨끗하게 빌드한 Moonlight.exe, AntiHooking.dll, Qt 및 의존 DLL, 실행기, 한국어 안내, 라이선스와 소스 위치.
- 02 호스트 ZIP: 현재 Windows 기본 바로가기 설치 스크립트, 한국어 이름의 실행 파일(CMD), 한국어 안내. 추가 EXE/DLL은 없습니다.
- `licenses/`: Qt 6.12.0과 Moonlight Windows 의존 라이브러리 v20의 라이선스·고지·소스 참조. 버전이 바뀌면 갱신합니다.

## 패키지 다시 만들기

1. [BUILDING.md](../BUILDING.md)에 따라 **공개용 소스**로 클라이언트를 빌드합니다. 개인 사용자명이 없는 빌드 경로를 쓰고 결과 EXE/DLL에도 개인 경로가 남지 않았는지 검사합니다.
2. 새로운 배포 폴더에 Moonlight.exe와 같은 빌드의 AntiHooking.dll을 복사합니다.
3. 같은 Qt 버전의 `windeployqt --release --no-compiler-runtime --no-system-d3d-compiler --no-opengl-sw --qmldir <소스의 app> <배포 폴더의 Moonlight.exe>`를 실행합니다.
4. 의존 라이브러리 v20의 x64 DLL과 Microsoft 정식 Visual C++ 재배포 런타임 DLL을 넣습니다. 디버그 DLL, PDB, 이전 EXE는 넣지 않습니다.
5. Python 3.9 이상에서 다음 명령을 실행합니다. 출력 폴더는 저장소 밖의 새 폴더여야 합니다.

```text
python scripts/package-windows-release.py --runtime <깨끗한 배포 폴더> --output <새 출력 폴더> --tag <배포 태그>
```

**사용 중인 Moonlight 폴더를 통째로 ZIP으로 만들지 마세요.** 페어링 키·설정·로그가 섞일 수 있습니다. 패키징 스크립트는 이런 파일을 거부하며 빈 portable.dat만 새로 만듭니다.

## 업로드 전

- 소스의 `scripts/check-public.ps1`로 소스·문서를 검사합니다.
- 두 ZIP의 내용을 풀어 개인키·개인 설정·주소·사용자명·개인 경로를 비교 검사하고 Gitleaks로 검사합니다.
- 01 ZIP을 **다른 폴더**에 풀어 `app/Moonlight.exe --version`과 `--help`가 정상 종료하는지 확인합니다. 필요하면 별도 테스트 복사본으로 앱 초기화를 검사합니다. 검사 중 생긴 개인 설정·로그를 배포 ZIP에 다시 넣지 않습니다.
- 02 ZIP의 VBS는 `/test` 인자로 비활성 테스트 폴더에 바로가기를 생성해 D/E/H, 대상, 인자를 확인할 수 있습니다. 이 검사는 실제 호스트 전환을 뜻하지 않습니다.
- `SHA256SUMS.txt`를 ZIP들과 함께 같은 비공개 Release에 올립니다. 태그의 소스와 실행 파일의 소스가 일치해야 합니다.
- 노트북에서 필압·호스트 버튼을 직접 확인한 결과와 빌드/자동 검사 결과를 구분해 릴리스 노트에 기록합니다.
