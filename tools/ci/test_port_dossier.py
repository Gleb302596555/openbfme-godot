import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import port_dossier as pd  # noqa: E402


def test_the_dossier_names_the_classes_and_their_open_functions():
    text = pd.dossier("Port 292: HordeContain's horde members; RW 0x870D4E sets the terrain decal")
    assert text.startswith(pd.MARK)
    assert "`HordeContain`" in text and "### Карта функций" in text


def test_a_text_without_a_class_or_an_address_gets_no_dossier():
    assert "досье не собрано" in pd.dossier("Local 99: a docs task")


def test_the_address_form():
    assert pd.rw("0x00892C33") == "RW 0x892C33"
