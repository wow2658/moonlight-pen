# Git 이력과 원본 비교

이 저장소는 처음에 검사한 소스 파일을 복사한 뒤 새 Git 저장소로 시작했습니다. 2026-11-15에 기존 프로젝트 커밋을 유지하면서 기반 Moonlight 포크의 이력을 조상으로 연결했습니다.

| 항목 | 연결한 대상 |
| --- | --- |
| Moonlight 기반 이력 | `d75068ee6acd848fad09d02b57871fd598e5e158`까지 3,440개 커밋 |
| 기존 프로젝트 이력 | `cd8fa5642da7c5dd71689df2ffb55225ee2849f4`까지 7개 커밋 |
| 이력 연결 커밋 | `12b4b955960e31abbc7fa3b0973f2f04a0bf8d1b` |

이력 연결에는 현재 파일 트리를 유지하는 merge를 사용했습니다. 연결 직후 Git 트리 ID가 이전 HEAD와 동일하며, 원본과 기존 프로젝트의 커밋 ID를 다시 쓰지 않았습니다. 그 뒤 이 문서와 출처 정보를 갱신했습니다. 프로그램 코드와 의존 소스 구성은 유지했습니다.

## 원격 저장소

| 이름 | 용도 |
| --- | --- |
| `origin` | 이 프로젝트의 GitHub 저장소 `wow2658/moonlight-pen` |
| `upstream` | 공식 Moonlight Qt `moonlight-stream/moonlight-qt` |
| `pen-upstream` | 기반 펜 포크 `AaaUpo/moonlight-qt-surface` |

이번 연결은 로컬 개발 저장소에서 정확한 기반 커밋의 조상만 가져왔습니다. 개발 폴더의 미커밋 수정·빌드 결과·개인 설정은 가져오지 않았으며, 최신 업스트림 코드를 새로 적용하는 작업도 수행하지 않았습니다. GitHub가 표시하는 Fork 관계는 별도의 저장소 메타데이터이며, 로컬 이력 연결로 변경되지 않습니다.

## 확인하는 명령

```sh
git log --graph --oneline --all
git merge-base --is-ancestor d75068ee6acd848fad09d02b57871fd598e5e158 HEAD
git merge-base --is-ancestor cd8fa5642da7c5dd71689df2ffb55225ee2849f4 HEAD
git diff --stat d75068ee6acd848fad09d02b57871fd598e5e158 HEAD
```

두 `merge-base` 명령의 종료 코드가 0이면 각각 원본 기반과 기존 프로젝트 HEAD가 현재 브랜치의 조상입니다. `--first-parent`로 로그를 보면 프로젝트 쪽 흐름만 표시되므로 원본 커밋 수천 개를 모두 보려면 해당 옵션 없이 확인합니다.

## 원본 업데이트를 가져올 때

공식 원본 변경은 `upstream`, 펜 포크 변경은 `pen-upstream`에서 fetch한 뒤 별도 작업 브랜치에서 검토합니다. 이력 연결만으로 과거 기반 이후의 변경이 자동 적용되지는 않습니다.

현재 의존 소스는 서브모듈을 일반 파일로 포함한 구성을 유지합니다. 원본의 서브모듈 리비전이나 `.gitmodules` 변경을 병합할 때는 이 차이를 함께 검토해야 합니다. 출처와 리비전은 `SOURCE_ORIGINS.json`에 있습니다.

기존 GitHub HEAD는 현재 브랜치의 조상이므로, 원격에 다른 변경이 없다면 이력 연결을 반영할 때 강제 푸시가 필요하지 않습니다. 로컬 작업과 원격 반영은 구분해서 확인합니다.
