# Moonlight Pen

**노트북의 S펜 또는 PTH-451로 데스크톱 PC에 그림을 그리기 위한 Windows용 Moonlight 수정판입니다.**
기기별 필압 보정과 화면 제어 패널을 추가했습니다. Moonlight 공식 배포판은 아닙니다.

## 다시 설치할 때는 여기부터

**[실행용 다운로드 열기 →](https://github.com/wow2658/moonlight-pen/releases/latest)**

이 저장소에 접근할 수 있는 GitHub 계정으로 로그인하세요. 비공개 저장소라 로그아웃 상태에서는 404가 보일 수 있습니다.

| 받을 파일 | 어느 컴퓨터에서? | 압축을 푼 뒤 할 일 |
| --- | --- | --- |
| [01-Laptop-Moonlight-Pen.zip](https://github.com/wow2658/moonlight-pen/releases/latest/download/01-Laptop-Moonlight-Pen.zip) | **펜을 사용하는 노트북** | `문라이트 실행.cmd` 더블클릭 |
| [02-Host-Display-Shortcuts.zip](https://github.com/wow2658/moonlight-pen/releases/latest/download/02-Host-Display-Shortcuts.zip) | **그리기 프로그램이 있는 데스크톱 PC** | `호스트 설치.cmd` 한 번 실행 |

**GitHub의 초록색 `Code → Download ZIP`과 Releases의 `Source code`는 개발용 소스입니다. 실행하려면 위의 01/02 ZIP을 받으세요.**

이미 지금 구성으로 잘 쓰고 있다면 재설치할 필요는 없습니다. 아래 안내는 새 PC에 설치하거나 나중에 복구할 때 사용하세요.

## 1. 데스크톱 PC에서 준비하기 — 처음 한 번

여기서 **호스트**란 화면을 보내 주는 PC, 즉 그리기 프로그램이 설치된 데스크톱을 뜻합니다.

1. Sunshine이 이미 설치되어 있고 화면 연결도 된다면 그대로 사용합니다. 새 Intel/AMD Windows PC라면 공식 [Sunshine-Windows-AMD64-installer.msi](https://github.com/LizardByte/Sunshine/releases/latest/download/Sunshine-Windows-AMD64-installer.msi)를 받아 설치하고, 안내에 따라 **Sunshine의 로컬 관리 계정**을 만듭니다. 파일을 찾을 수 없으면 [공식 Releases](https://github.com/LizardByte/Sunshine/releases/latest)에서 Windows AMD64 **installer**를 고릅니다. ARM64·lite·debuginfo 파일은 이 안내의 대상이 아닙니다.
2. `02-Host-Display-Shortcuts.zip`을 **데스크톱 PC에** 다운로드합니다.
3. ZIP을 오른쪽 클릭해 **모두 압축 풀기**를 선택합니다. ZIP 안에서 바로 실행하지 마세요.
4. 풀린 폴더에서 **`호스트 설치.cmd`**를 더블클릭합니다.
5. 검은 창에 **`PASS: created and verified all three shortcuts.`**가 나오고 `Clone / Extend / Check` 바로가기 폴더가 열리면 등록이 끝났습니다.

Sunshine을 이 수정판에 맞춰 재설치하거나 패치할 필요는 없습니다. 02 ZIP은 **복제·확장 버튼용**입니다. 펜 필압만 사용할 때는 생략할 수 있습니다.

## 2. 노트북에서 문라이트 실행하기

1. `01-Laptop-Moonlight-Pen.zip`을 **노트북에** 다운로드합니다.
2. **모두 압축 풀기**를 누르고, 풀린 폴더를 문서 등 계속 보관할 위치에 둡니다.
3. 기존 Moonlight를 종료한 뒤 **`문라이트 실행.cmd`**를 더블클릭합니다.
4. 문라이트에 데스크톱 PC가 나타나면 선택합니다. 안 보이면 `+`로 호스트의 IP를 입력합니다.
5. 처음 연결하면 문라이트가 보여 주는 **PIN을 호스트 PC의 Sunshine 웹 화면 → PIN에 입력**해 페어링합니다. 이 단계는 두 기기를 서로 연결하는 절차입니다.
6. 문라이트에서 **Desktop**을 실행합니다. 노트북에 데스크톱 화면이 보이면 연결 성공입니다.

`문라이트 실행.cmd`와 `app` 폴더는 함께 두세요. 바탕화면에서 실행하고 싶다면 CMD 파일의 **바로가기**를 만드세요. CMD나 EXE 파일 하나만 따로 옮기면 실행되지 않습니다.

이 패키지는 빈 설정으로 시작합니다. 이전 PC의 인증서나 개인 설정은 들어 있지 않으므로, 새 설치에서는 직접 페어링해야 합니다.

## 3. 펜과 화면 버튼이 되는지 확인하기

| 확인할 것 | 어디서, 무엇을 하나요? | 정상 결과 |
| --- | --- | --- |
| 필압 | 연결된 호스트에서 그리기 앱을 열고 약하게/강하게 선을 그립니다. | 브러시 굵기나 진하기가 달라집니다. |
| 입력·전송 막대 | 연결 중 `Ctrl+Alt+Shift+P`를 누르고 펜을 눌러 봅니다. | **IN / OUT 두 막대**가 움직입니다. 다시 누르면 숨겨집니다. |
| 화면 패널 | 연결 중 `Ctrl+Alt+Shift+B`를 누릅니다. | 모니터·복제·확장 버튼이 보입니다. |
| 호스트 연결 확인 | 패널의 **`?`**를 누릅니다. | **호스트 PC의 Windows 디스플레이 설정**이 열립니다. |
| 복제·확장 | 패널에서 **복제** 또는 **확장**을 누릅니다. | **호스트 PC**의 화면 모드가 바뀝니다. 노트북 화면 모드를 바꾸는 버튼이 아닙니다. |

필압 막대는 기본으로 꺼져 있습니다. 숨겨도 필압 보정은 계속 적용됩니다. IN/OUT 막대가 움직이는 것은 노트북의 입력·전송 값을 확인한 것이며, 그리기 앱의 필압 동작은 직접 선을 그려 확인합니다.

**PTH-451은 노트북에 연결합니다.** [Wacom 공식 드라이버](https://www.wacom.com/support/product-support/drivers)에서 모델 **PTH-451**을 검색해 지원되는 Windows 드라이버를 노트북에 설치하고, **와콤 타블렛 등록정보 → 펜 → 매핑 → Windows Ink 사용**을 켜세요. 호스트의 그리기 앱도 Windows Ink/태블릿 PC 입력을 사용하도록 설정합니다. [Wacom PTH-451 공식 설명서](https://101.wacom.com/UserHelpPDF_Legacy/PTH-451_en.pdf)

## 필압을 내 손에 맞추기

문라이트 연결을 끊고 **설정(톱니바퀴) → 펜 필압 설정**을 엽니다. 바꾼 값은 **다음 연결부터** 적용됩니다.

| 항목 | 기본값 / 사용법 |
| --- | --- |
| 사용할 필압 설정 | **자동**. 내장 펜은 S펜, 외장 타블렛은 PTH-451. 잘못 선택되면 직접 지정합니다. |
| S펜 · 100%에 도달하는 입력 압력 | **70%**. 펜을 최대 힘의 70%까지 누르면 전송 필압이 100%가 됩니다. |
| PTH-451 · 100%에 도달하는 입력 압력 | **100%**. 필요하면 20~100% 사이에서 조절합니다. |
| 중간 필압 감도 | 왼쪽으로: 더 부드럽게. **1.00**: 직선 비례. 오른쪽으로: 더 단단하게. |
| 필압 기본값 복원 | S펜 70% / 감도 0.50, PTH-451 100% / 감도 1.00, 필압 표시 꺼짐으로 되돌립니다. |

## 평소에 기억할 단축키

모두 **노트북에서 스트리밍 연결 중** 누릅니다.

| 단축키 | 기능 |
| --- | --- |
| `Ctrl+Alt+Shift+B` | 화면 제어 패널 열기 / 접기 |
| `Ctrl+Alt+Shift+P` | 필압 IN/OUT 표시 켜기 / 끄기 |
| `Ctrl+Alt+Shift+O` | 성능 통계 표시 켜기 / 끄기 — 이 수정판은 S 대신 **O** |

패널의 **모니터 1 / 2**는 스트리밍할 화면 선택입니다. Sunshine이 나열하는 순서라 Windows 설정의 화면 번호와 다를 수 있습니다.

## 막히면 / 나중에 다시 꺼내 볼 때

- **[문제 해결과 재설치 안내](docs/TROUBLESHOOTING.md)** — 창이 안 뜸, 필압 안 됨, 복제·확장 무반응, PC 교체, 설정 보관.
- **[개발자용 빌드 안내](BUILDING.md)** — 소스를 수정하고 직접 EXE를 만들 때만 읽으면 됩니다.
- [변경과 출처](NOTICE.md) · [검사 기록](PUBLICATION_SCAN.md) · [원본 Moonlight 안내](README.upstream.md)

## 라이선스

Moonlight Qt와 이 수정판은 [GNU GPL v3](LICENSE) 조건으로 배포합니다. 실행용 ZIP의 `licenses`와 `SOURCE.txt`에 포함 라이브러리의 고지와 대응 소스 위치를 담았습니다. 의존 소스 출처·리비전은 [SOURCE_ORIGINS.json](SOURCE_ORIGINS.json)에 기록했습니다.
