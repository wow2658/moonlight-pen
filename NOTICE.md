# 출처와 변경 고지

- 원본: https://github.com/moonlight-stream/moonlight-qt
- 기반 커뮤니티 펜 포크: https://github.com/AaaUpo/moonlight-qt-surface
- 기반 커밋: d75068ee6acd848fad09d02b57871fd598e5e158
- 관련 작업: https://github.com/moonlight-stream/moonlight-qt/pull/2088 및 https://github.com/AaaUpo/moonlight-qt-surface/pull/1
- 로컬 수정 날짜: 2026-11-15

로컬 변경: Windows 펜 프로필·압력 곡선·도달 압력 설정, IN/OUT 패널, 모니터 전환 및 Windows 호스트 도우미, 관련 빌드·검증 스크립트와 문서.
성능 표시 단축키를 Ctrl+Alt+Shift+S에서 Ctrl+Alt+Shift+O로 바꾸고 설정 화면의 설명을 갱신했습니다.
호스트의 Windows 기본 DisplaySwitch와 시작 메뉴 단축키를 이용하는 텍스트 스크립트 설치 대안을 추가했습니다.

Moonlight 공식판이 아니며 원본 프로젝트의 보증·승인을 의미하지 않습니다. 원본 저작권·라이선스 문구를 보존했습니다. 루트 LICENSE는 GPL v3입니다.

기반 커뮤니티 펜 포크의 커밋 d75068ee6acd848fad09d02b57871fd598e5e158까지의 Moonlight 이력을 현재 브랜치의 조상으로 연결했습니다. 기존 프로젝트 커밋과 수정 파일을 보존했습니다. 의존 Git 저장소의 추적 파일은 소스 형태로 포함하며, 각 의존 저장소의 별도 Git 이력은 편입하지 않았습니다. 저장소별 출처·커밋과 이력 연결 정보는 SOURCE_ORIGINS.json에 기록했습니다. 각 의존 디렉터리의 LICENSE/COPYING 고지를 유지했습니다.

소스 저장소 구성: 원본 README를 README.upstream.md로 보존했습니다. 원본 GitHub 자동 실행·배포 설정과 .gitmodules는 현재 작업 트리에 포함하지 않았습니다. Git으로 추적하는 파일은 소스·문서·패키징 자료입니다. 실행 파일과 서드파티 DLL은 검사한 실행용 ZIP으로 GitHub Releases에 별도 제공합니다.

호스트 명령 v2: Sunshine이 소비하는 F13 조합을 피하고 일반 키 Ctrl+Alt+Shift+D/E/H로 전환했습니다. 클라이언트 로그에는 버튼 요청과 전송 큐 실패 여부를 기록하며, 서버 실행 성공으로 간주하지 않습니다.
패널 v3: 버튼의 배경 지우기를 어두운 색으로 처리하고 각 버튼을 메모리에 완성한 후 표시하도록 변경했습니다. 위치만 바뀌면 패널을 재배치하지 않고, 같은 버튼 안의 마우스 이동은 다시 그리지 않도록 수정했습니다. 호스트 명령은 v2와 같습니다.

Git 이력 구성과 원본 비교 방법은 docs/GIT_HISTORY.md를 참고하세요. 과거 원본 커밋에는 당시의 파일 구성이 그대로 보존됩니다.
