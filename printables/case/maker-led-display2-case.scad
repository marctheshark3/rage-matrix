// =============================================================================
// maker-led-display2-case v0.1.1 — snug two-piece for Soldered Display2
// OpenSCAD 2021.01 · mm · P1S · mule (v2.1 .brd XY + 8.5 mm header Z)
// =============================================================================
// product_class: enclosure
// print_orientation: feet-down
// print_up_axis: Z
// soft_mode: yes
// expected_components: 1  (per exported STL: base XOR bezel)
// version: v0.1.1
//
// Eagle origin BL → CAD centered: cad = eagle - (52, 19.5)
// Bezel: face on bed (z=0); stop ring +Z only.
// v0.1.1: bezel cutouts Y-mirrored so after face-on-bed flip the
// header poke lands on the 1x6 (v0.1.0 poke sat on USB). Base unchanged.
// =============================================================================

include <lib/soft_helpers.scad>

/* [Export] */
which = "assembly"; // base | bezel | assembly

/* [Board — from-brd-v2.1 / from-user] */
pcb_x = 104.0;
pcb_y = 39.0;
pcb_t = 1.6;
clear = 0.8;

/* [Shell] */
wall = 2.4;
floor_t = 2.2;
seat_h = 2.0;
above_pcb = 10.0;     // header 8.5 + 1.5 slack
r_out = 4.0;
face_t = 2.2;
stop_h = 2.8;
min_feat = 1.6;

/* [LED window — from-brd-v2.1 field + margin] */
win_x = 82.0;
win_y = 24.0;
win_cx = -9.6;
win_cy = -6.66; // board frame (LED face, +Y = electronics strip)

/* [Header poke — connector-standard 1x6] */
hdr_cx = 40.8;
hdr_cy = 17.4; // board frame
hdr_w = 18.0;
hdr_h = 8.0;

// Face-on-bed bezel is flipped onto the base (stop −Z). That inverts Y
// vs the board. Pre-mirror cutouts so assembled poke hits the 1x6.
bezel_flip_y = -1;

/* [I/O keepouts — locked DESIGN.md] */
usb_y = 11.3;
usb_w = 13.0;
usb_h = 8.5;

sw_y = 11.0;
sw_w = 11.0;
sw_h = 6.0;

easyc_x = -39.9;
easyc_w = 10.0;
easyc_h = 8.0;

jst_x = -7.0;
jst_w = 10.0;
jst_h = 8.0;

/* [Posts — outside PCB XY] */
post_od = 5.2;
post_id = 2.4;

$fn = 48;
eps = 0.06;

ix = pcb_x + 2 * clear;
iy = pcb_y + 2 * clear;
ox = ix + 2 * wall;
oy = iy + 2 * wall;
pcb_z0 = floor_t + seat_h;
base_z = pcb_z0 + pcb_t + above_pcb;
mh_x = ix / 2 + wall / 2;
mh_y = iy / 2 + wall / 2;
io_z0 = pcb_z0 + pcb_t + 0.4;

module outer2d() soft_rect(ox, oy, r_out);
module cavity2d() soft_rect(ix, iy, max(1.0, r_out - wall));

module soft_port(axis, loc, w, h, z_mid) {
  rr = min(1.2, min(w, h) / 2 - 0.05);
  if (axis == "xneg")
    translate([-ox / 2 - 2, loc, z_mid])
      rotate([90, 0, 90])
        linear_extrude(height = wall + 4)
          soft_rect(w, h, rr);
  else if (axis == "xpos")
    translate([ox / 2 - 2, loc, z_mid])
      rotate([90, 0, 90])
        linear_extrude(height = wall + 4)
          soft_rect(w, h, rr);
  else if (axis == "ypos")
    translate([loc, iy / 2 - 2, z_mid])
      rotate([90, 0, 0])
        linear_extrude(height = wall + 6)
          soft_rect(w, h, rr);
}

module base() {
  difference() {
    linear_extrude(height = base_z)
      outer2d();

    translate([0, 0, floor_t])
      linear_extrude(height = base_z)
        cavity2d();

    soft_port("xneg", usb_y, usb_w, usb_h, io_z0 + usb_h / 2);
    soft_port("xpos", sw_y, sw_w, sw_h, io_z0 + sw_h / 2);
    soft_port("ypos", easyc_x, easyc_w, easyc_h, io_z0 + easyc_h / 2);
    soft_port("ypos", jst_x, jst_w, jst_h, io_z0 + jst_h / 2);

    for (sx = [-1, 1], sy = [-1, 1])
      translate([sx * mh_x, sy * mh_y, -eps])
        cylinder(d = post_id, h = base_z + 1);
  }

  // Corner seats — PCB rests here (solder-tail clearance under midspan)
  for (sx = [-1, 1], sy = [-1, 1])
    translate([sx * (pcb_x / 2 - 4), sy * (pcb_y / 2 - 4), floor_t])
      linear_extrude(height = seat_h)
        soft_rect(8, 8, 1.2);

  for (sx = [-1, 1], sy = [-1, 1])
    difference() {
      translate([sx * mh_x, sy * mh_y, 0])
        cylinder(d = post_od, h = base_z);
      translate([sx * mh_x, sy * mh_y, -eps])
        cylinder(d = post_id, h = base_z + 1);
    }
}

module bezel() {
  difference() {
    union() {
      linear_extrude(height = face_t)
        outer2d();
      translate([0, 0, face_t - eps])
        linear_extrude(height = stop_h)
          difference() {
            offset(r = -0.35)
              cavity2d();
            offset(r = -2.6)
              cavity2d();
          }
    }
    translate([win_cx, bezel_flip_y * win_cy, -1])
      linear_extrude(height = face_t + stop_h + 2)
        soft_rect(win_x, win_y, 1.6);
    translate([hdr_cx, bezel_flip_y * hdr_cy, -1])
      linear_extrude(height = face_t + stop_h + 2)
        soft_rect(hdr_w, hdr_h, 1.2);
    for (sx = [-1, 1], sy = [-1, 1])
      translate([sx * mh_x, sy * mh_y, -1])
        cylinder(d = post_id, h = face_t + stop_h + 2);
  }
}

module assembly() {
  base();
  translate([0, 0, base_z + 1])
    bezel();
}

echo(str("VERSION=v0.1.1 class=enclosure feet-down mule bezel_flip_y=", bezel_flip_y));
echo(str("outer=", ox, "x", oy, "x", base_z, " cavity=", ix, "x", iy));
echo(str("pcb_z0=", pcb_z0, " above_pcb=", above_pcb, " header_poke=", hdr_w, "x", hdr_h));
echo(str("USB=", usb_w, "x", usb_h, " SW=", sw_w, "x", sw_h, " easyC/JST=", easyc_w, "x", easyc_h));
echo(str("win=", win_x, "x", win_y, " @", win_cx, ",", bezel_flip_y * win_cy));
echo(str("hdr_poke @", hdr_cx, ",", bezel_flip_y * hdr_cy, " (board-frame y=", hdr_cy, ")"));
echo("ORIENT feet-down base; bezel face-on-bed stop +Z");
echo(str("P1S=", (ox < 250 && oy < 250), " min_feat=", min_feat));

if (which == "base") base();
else if (which == "bezel") bezel();
else assembly();
