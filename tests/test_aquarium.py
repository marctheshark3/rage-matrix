from pathlib import Path

from sim.aquarium import Aquarium, VIEW_H, VIEW_W, WORLD_H, WORLD_W


ROOT = Path(__file__).resolve().parents[1]


def test_seeded_aquarium_is_deterministic_and_uses_larger_world():
    a = Aquarium(seed=1337)
    b = Aquarium(seed=1337)

    assert (WORLD_W, WORLD_H) > (VIEW_W, VIEW_H)
    assert a.state() == b.state()
    assert a.px_hex() == b.px_hex()
    assert len(a.px_hex()) == VIEW_W * VIEW_H * 2


def test_fish_swim_side_to_side_and_through_depth_without_leaving_world():
    aquarium = Aquarium(seed=7)
    before = [(fish.x, fish.z, fish.direction) for fish in aquarium.fish]

    for _ in range(240):
        aquarium.step()

    assert any(abs(fish.x - x) > 1.0 for fish, (x, _, _) in zip(aquarium.fish, before))
    assert any(abs(fish.z - z) > 0.05 for fish, (_, z, _) in zip(aquarium.fish, before))
    assert all(1.0 <= fish.x <= WORLD_W - 2.0 for fish in aquarium.fish)
    assert all(0.06 <= fish.z <= 0.98 for fish in aquarium.fish)
    assert all(fish.direction in (-1, 1) for fish in aquarium.fish)


def test_depth_controls_brightness_and_apparent_glyph_size():
    aquarium = Aquarium(seed=11, fish_count=0)
    far = aquarium.add_fish(18.0, 5.0, 0.05, 1)
    near = aquarium.add_fish(25.0, 5.0, 0.95, -1)
    aquarium.cam_x = 8.0
    aquarium.cam_y = 0.0

    fb = aquarium.render()
    far_x = round(far.x - aquarium.cam_x)
    near_x = round(near.x - aquarium.cam_x)
    far_pixels = sum(v > 0 for row in fb for v in row[max(0, far_x - 2) : far_x + 3])
    near_pixels = sum(v > 0 for row in fb for v in row[max(0, near_x - 2) : near_x + 3])
    far_peak = max(row[far_x] for row in fb)
    near_peak = max(row[near_x] for row in fb)

    assert near_peak > far_peak
    assert near_pixels > far_pixels


def test_camera_moves_but_stays_bounded_and_world_has_sparse_scenery():
    aquarium = Aquarium(seed=19)
    start = aquarium.cam_x
    for fish in aquarium.fish:
        fish.x = WORLD_W - 5.0
    for _ in range(80):
        aquarium.step()

    assert aquarium.cam_x > start
    assert 0.0 <= aquarium.cam_x <= WORLD_W - VIEW_W
    assert 0.0 <= aquarium.cam_y <= WORLD_H - VIEW_H
    assert 0 < len(aquarium.plants) <= 8
    assert 0 < len(aquarium.rocks) <= 8
    assert 0 < len(aquarium.bubbles) <= 12


def test_hub_uses_aquarium_twin_instead_of_other_twin_backends(monkeypatch):
    from hub import matrix_api

    class WrongTwin:
        @staticmethod
        def available():
            return True

        @staticmethod
        def pull(view):
            return {"sim": "tank"}, ""

    monkeypatch.setattr(matrix_api, "_view", "aquarium")
    monkeypatch.setattr(matrix_api, "_aquarium", None)
    monkeypatch.setattr(matrix_api, "_rust", lambda: WrongTwin())
    monkeypatch.setattr(matrix_api, "_http", lambda *args, **kwargs: (_ for _ in ()).throw(OSError("offline")))

    state, px = matrix_api.pull_fb("aquarium")

    assert state["sim"] == "aquarium"
    assert len(px) == VIEW_W * VIEW_H * 2


def test_firmware_and_hub_expose_sealed_aquarium_mode_without_switch_reseed():
    main = (ROOT / "src/main.cpp").read_text()
    hub = (ROOT / "hub/matrix_api.py").read_text()
    page = (ROOT / "hub/static/index.html").read_text()

    assert '#include "aquarium.h"' in main
    assert "MODE_AQUARIUM" in main
    assert "aquariumBegin();" in main
    assert "aquariumStep();" in main
    assert "aquariumRender(fb, brightScale);" in main
    enter_mode = main[main.index("static void enterMode(Mode m)") : main.index("static void printHelp()")]
    assert "aquariumSeed" not in enter_mode
    assert "crittersBegin" not in enter_mode
    assert "warSeed" not in enter_mode
    assert '"aquarium"' in hub
    assert '"name":"aquarium"' in page
