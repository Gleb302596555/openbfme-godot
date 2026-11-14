# Эта машина: Google Colab (Linux), рабочая область colab-sandbox
- Сначала прочитай CLAUDE.md, AGENTS.md и docs/ennorath/PROCESS.md (разделы 5, 7, 14): они обязательны.
- Сборка только `bash tools/colab/build_tests.sh` из своего дерева (CMake + Ninja + GCC, движок, тесты и GDExtension). build.bat и MSVC здесь нет.
  Ошибка в engine/src/GodotDevice/ ломает игру владельца на Windows: собирай с GDExtension всегда.
- `/content/sandbox` стирается при перезапуске Colab (это не Google Drive): всё, что не запушено, пропадает.
  Работай только в своей ветке (не в дереве без ветки), коммит и push после каждого шага.
- Эталоны хешей (pins) и перепись стопов не перемерять: их перемеряет Claude. Красный pin-тест - сообщить, не менять.
- x87_oracle / retail_oracle (Windows) здесь не работают: такие тесты не считать ни зелёными, ни красными.
- Файлы игры в /content/game_assets, game.dat RotWK 2.01 - в $OPENBFME_GAME_DAT. Код RotWK читать по нему (pefile + capstone,
  адреса RW = виртуальные адреса game.dat). Сначала декомпиляция BFME2 (Open-BFME-2), потом каждое звено подтвердить в RW.
- game.dat и любые файлы игры НИКОГДА не коммитить, не класть в репозиторий, в workflow GitHub Actions, в артефакты и в issue/PR.
- Диск стирается в конце сессии: коммит и `git push origin <своя ветка>` после каждого законченного шага. В main не пушить.
- Коммит только через хук tools/precommit.py, явные пути в git add, без --no-verify.
- Узкие фильтры тестов (`engine/build/openbfme_tests -tc="..."`); полный прогон не запускать без слова владельца.
- Отчёт владельцу по-русски.
