//! 64×18 torus pred–prey. 32×9 camera. No heap in `step`.
use crate::{clamp, fb_hex, steer, wrap, wrapd, Rng, VH, VW};

pub const WW: f32 = 64.0;
pub const WH: f32 = 18.0;
pub const MAX_C: usize = 14;
pub const MAX_F: usize = 20;
pub const KIND_PREY: u8 = 0;
pub const KIND_PRED: u8 = 1;

#[derive(Clone, Copy, Default)]
pub struct Critter {
    pub x: f32,
    pub y: f32,
    pub a: f32,
    pub e: f32,
    pub g: [u8; 8],
    pub gen: u8,
    pub kind: u8,
    pub alive: bool,
}

#[derive(Clone, Copy, Default)]
pub struct Food {
    pub x: f32,
    pub y: f32,
    pub a: f32,
    pub on: bool,
}

pub struct Tank {
    pub rng: Rng,
    pub bright: u8,
    pub follow: bool,
    pub cam_x: f32,
    pub cam_y: f32,
    pub shake: f32,
    pub births: u32,
    pub tick: u32,
    critters: [Critter; MAX_C],
    foods: [Food; MAX_F],
    trail: [u8; VW * VH],
}

impl Tank {
    pub fn new(seed: u32) -> Self {
        let mut t = Self {
            rng: Rng::new(seed),
            bright: 90,
            follow: true,
            cam_x: 16.0,
            cam_y: 4.5,
            shake: 0.0,
            births: 0,
            tick: 0,
            critters: [Critter::default(); MAX_C],
            foods: [Food::default(); MAX_F],
            trail: [0; VW * VH],
        };
        t.seed();
        t
    }

    fn mutate(&mut self, src: [u8; 8]) -> [u8; 8] {
        let mut g = src;
        for _ in 0..2 {
            let i = self.rng.range_u32(8) as usize;
            let v = g[i] as i32 + self.rng.i8(-25, 25);
            g[i] = v.clamp(0, 255) as u8;
        }
        g
    }

    fn rand_g(&mut self) -> [u8; 8] {
        let mut g = [0u8; 8];
        for e in &mut g {
            *e = 40 + self.rng.range_u32(181) as u8;
        }
        g
    }

    fn slot_c(&self) -> Option<usize> {
        self.critters.iter().position(|c| !c.alive)
    }

    fn spawn(&mut self, x: f32, y: f32, src: Option<[u8; 8]>, gen: u8, e: f32, kind: u8) -> bool {
        if self.alive() >= MAX_C as u8 {
            return false;
        }
        let Some(i) = self.slot_c() else {
            return false;
        };
        let g = match src {
            Some(s) => self.mutate(s),
            None => self.rand_g(),
        };
        self.critters[i] = Critter {
            x: wrap(x, WW),
            y: wrap(y, WH),
            a: self.rng.f01() * core::f32::consts::TAU,
            e,
            g,
            gen,
            kind,
            alive: true,
        };
        self.births += 1;
        true
    }

    fn drop(&mut self, x: f32, y: f32, a: f32) {
        let a = clamp(a, 0.25, 1.2);
        if let Some(i) = self.foods.iter().position(|f| !f.on) {
            self.foods[i] = Food {
                x: wrap(x, WW),
                y: wrap(y, WH),
                a,
                on: true,
            };
            return;
        }
        // overwrite a random pellet if full
        let i = self.rng.range_u32(MAX_F as u32) as usize;
        self.foods[i] = Food {
            x: wrap(x, WW),
            y: wrap(y, WH),
            a,
            on: true,
        };
    }

    pub fn seed(&mut self) {
        self.critters = [Critter::default(); MAX_C];
        self.foods = [Food::default(); MAX_F];
        self.trail = [0; VW * VH];
        self.births = 0;
        self.cam_x = 16.0;
        self.cam_y = 4.5;
        self.shake = 0.0;
        self.tick = 0;
        for _ in 0..8 {
            let x = 6.0 + self.rng.f01() * 52.0;
            let y = 2.0 + self.rng.f01() * 14.0;
            let e = 0.65 + self.rng.f01() * 0.35;
            self.spawn(x, y, None, 0, e, KIND_PREY);
        }
        for _ in 0..3 {
            let x = self.rng.f01() * WW;
            let y = self.rng.f01() * WH;
            let e = 0.85 + self.rng.f01() * 0.25;
            self.spawn(x, y, None, 0, e, KIND_PRED);
        }
        for _ in 0..10 {
            let x = self.rng.f01() * WW;
            let y = self.rng.f01() * WH;
            let a = 0.6 + self.rng.f01() * 0.4;
            self.drop(x, y, a);
        }
    }

    fn nearest_kind(&self, i: usize, kind: u8, rng: f32) -> Option<(usize, f32, f32, f32)> {
        let c = self.critters[i];
        let mut best = None;
        let mut bd = rng * rng;
        for (j, o) in self.critters.iter().enumerate() {
            if j == i || !o.alive || o.kind != kind {
                continue;
            }
            let dx = wrapd(o.x - c.x, WW);
            let dy = wrapd(o.y - c.y, WH);
            let d2 = dx * dx + dy * dy;
            if d2 < bd {
                bd = d2;
                best = Some((j, d2, dx, dy));
            }
        }
        best
    }

    fn nearest_food(&self, i: usize) -> Option<(usize, f32, f32, f32)> {
        let c = self.critters[i];
        let rng = 4.0 + (c.g[2] as f32 / 255.0) * 18.0;
        let mut best = None;
        let mut bd = rng * rng;
        for (j, f) in self.foods.iter().enumerate() {
            if !f.on {
                continue;
            }
            let dx = wrapd(f.x - c.x, WW);
            let dy = wrapd(f.y - c.y, WH);
            let d2 = dx * dx + dy * dy;
            if d2 < bd {
                bd = d2;
                best = Some((j, d2, dx, dy));
            }
        }
        best
    }

    pub fn step(&mut self) {
        self.tick += 1;
        if self.shake > 0.0 {
            self.shake *= 0.86;
        }
        if self.tick % 12 == 0 {
            let x = self.rng.f01() * WW;
            let y = self.rng.f01() * WH;
            let a = 0.45 + 0.30 * self.rng.f01();
            self.drop(x, y, a);
        }
        let mut n = 0u8;
        for i in 0..MAX_C {
            if !self.critters[i].alive {
                continue;
            }
            let pred = self.critters[i].kind == KIND_PRED;
            let g0 = self.critters[i].g[0] as f32 / 255.0;
            let g1 = self.critters[i].g[1] as f32 / 255.0;
            let g2 = self.critters[i].g[2] as f32 / 255.0;
            let g3 = self.critters[i].g[3] as f32 / 255.0;
            let g7 = self.critters[i].g[7] as f32 / 255.0;
            let mut speed =
                (if pred { 0.14 } else { 0.22 }) + g0 * (if pred { 0.26 } else { 0.38 });
            let turn = 0.14 + g1 * 0.50;
            let metab = if pred {
                0.00055 + g7 * 0.00055
            } else {
                0.00028 + g7 * 0.00032
            };
            let rng = 6.0 + g2 * (if pred { 18.0 } else { 12.0 });
            let conv = 0.70 + g3 * 0.35;
            let prey_split = 0.48 + g3 * 0.18;
            if self.shake > 0.05 {
                self.critters[i].a += (self.rng.f01() - 0.5) * self.shake * 2.4;
                speed += self.shake * 0.35;
            }
            if pred {
                if let Some((pj, pd2, pdx, pdy)) = self.nearest_kind(i, KIND_PREY, rng) {
                    steer(&mut self.critters[i].a, pdx, pdy, turn, 1.0);
                    if pd2 < 1.45 {
                        let add = 0.45 + self.critters[pj].e * conv;
                        self.critters[i].e = (self.critters[i].e + add).min(2.2);
                        self.drop(self.critters[pj].x, self.critters[pj].y, 0.18);
                        self.critters[pj].alive = false;
                        if self.pred_n() < 5 {
                            let pup = 0.55 + 0.15 * g3;
                            let gx = self.critters[i].g;
                            let gen = self.critters[i].gen.saturating_add(1);
                            let (x, y) = (self.critters[i].x, self.critters[i].y);
                            if self.spawn(x, y, Some(gx), gen, pup, KIND_PRED) {
                                self.critters[i].e = (self.critters[i].e - 0.22).max(0.35);
                            }
                        }
                    }
                } else {
                    self.critters[i].a += (self.rng.f01() - 0.5) * turn * 0.8;
                }
            } else {
                self.critters[i].a += (self.rng.f01() - 0.5) * (0.22 + g1 * 0.28);
                let hunter = self.nearest_kind(i, KIND_PRED, rng);
                let food = self.nearest_food(i);
                let school = self.nearest_kind(i, KIND_PREY, 9.0);
                if let Some((_, hd2, hdx, hdy)) = hunter {
                    if hd2 < 49.0 {
                        steer(&mut self.critters[i].a, -hdx, -hdy, turn, 1.25);
                        speed += 0.20;
                    } else {
                        if let Some((_, fd2, fdx, fdy)) = food {
                            if fd2 > 1.8 {
                                steer(&mut self.critters[i].a, fdx, fdy, turn, 0.45);
                            }
                        }
                        if let Some((_, sd2, sdx, sdy)) = school {
                            if (2.8..80.0).contains(&sd2) {
                                steer(&mut self.critters[i].a, sdx, sdy, turn, 0.30);
                            }
                        }
                    }
                }
                if let Some((fi, fd2, _, _)) = food {
                    if fd2 < 0.95 {
                        self.critters[i].e =
                            (self.critters[i].e + self.foods[fi].a * 0.62).min(1.8);
                        self.foods[fi].a -= 0.40;
                        if self.foods[fi].a < 0.12 {
                            self.foods[fi].on = false;
                        }
                        self.critters[i].a += 2.4 + (self.rng.f01() - 0.5) * 1.4;
                    }
                }
            }
            let a = self.critters[i].a;
            self.critters[i].x = wrap(self.critters[i].x + a.cos() * speed, WW);
            self.critters[i].y = wrap(self.critters[i].y + a.sin() * speed, WH);
            self.critters[i].e -= metab;
            if !pred && self.critters[i].e > prey_split && self.prey_n() < 10 {
                let g = self.critters[i].g;
                let gen = self.critters[i].gen.saturating_add(1);
                let (x, y) = (self.critters[i].x, self.critters[i].y);
                if self.spawn(x, y, Some(g), gen, 0.38, KIND_PREY) {
                    self.critters[i].e -= 0.28;
                }
            }
            if self.critters[i].e < 0.04 {
                if !pred {
                    self.drop(self.critters[i].x, self.critters[i].y, 0.28);
                }
                self.critters[i].alive = false;
                continue;
            }
            n += 1;
        }
        let prey_n = self.prey_n();
        let pred_n = self.pred_n();
        if pred_n == 0 && prey_n >= 6 && self.tick % 180 == 0 {
            let x = self.rng.f01() * WW;
            let y = self.rng.f01() * WH;
            self.spawn(x, y, None, 0, 0.9, KIND_PRED);
        }
        if prey_n == 0 && pred_n > 0 && self.tick % 120 == 0 {
            let x = self.rng.f01() * WW;
            let y = self.rng.f01() * WH;
            self.spawn(x, y, None, 0, 0.7, KIND_PREY);
        }
        if n == 0 {
            self.seed();
        }
        if self.follow {
            let mut sx = 0.0;
            let mut sy = 0.0;
            let mut k = 0.0;
            for c in &self.critters {
                if !c.alive {
                    continue;
                }
                if pred_n > 0 && c.kind != KIND_PRED {
                    continue;
                }
                sx += wrapd(c.x - self.cam_x, WW) + self.cam_x;
                sy += wrapd(c.y - self.cam_y, WH) + self.cam_y;
                k += 1.0;
            }
            if k > 0.0 {
                let tx = sx / k;
                let ty = sy / k;
                self.cam_x = wrap(
                    self.cam_x + wrapd(tx - VW as f32 * 0.5 - self.cam_x, WW) * 0.04,
                    WW,
                );
                self.cam_y = wrap(
                    self.cam_y + wrapd(ty - VH as f32 * 0.5 - self.cam_y, WH) * 0.04,
                    WH,
                );
            }
        }
        for t in &mut self.trail {
            *t = t.saturating_sub(4);
        }
    }

    pub fn feed(&mut self, vx: Option<i32>, vy: Option<i32>) {
        let x = self.cam_x + (vx.unwrap_or(4 + self.rng.range_u32(24) as i32) as f32) + 0.5;
        let y = self.cam_y + (vy.unwrap_or(1 + self.rng.range_u32(7) as i32) as f32) + 0.5;
        self.drop(x, y, 1.05);
        self.drop(x + 0.8, y - 0.4, 0.7);
    }

    pub fn scatter(&mut self) {
        for _ in 0..6 {
            let x = self.rng.f01() * WW;
            let y = self.rng.f01() * WH;
            self.drop(x, y, 0.8);
        }
    }

    pub fn shake(&mut self, amp: f32) {
        self.shake = clamp(amp, 0.3, 2.4);
        for c in &mut self.critters {
            if c.alive {
                c.a += (self.rng.f01() - 0.5) * 3.2 * amp;
                c.e *= 0.96;
            }
        }
        for f in &mut self.foods {
            if f.on {
                f.x = wrap(f.x + (self.rng.f01() - 0.5) * 4.0 * amp, WW);
                f.y = wrap(f.y + (self.rng.f01() - 0.5) * 2.2 * amp, WH);
            }
        }
    }

    pub fn drop_hunter(&mut self) {
        self.spawn(self.cam_x + 16.0, self.cam_y + 4.0, None, 0, 1.0, KIND_PRED);
    }

    pub fn alive(&self) -> u8 {
        self.critters.iter().filter(|c| c.alive).count() as u8
    }
    pub fn prey_n(&self) -> u8 {
        self.critters
            .iter()
            .filter(|c| c.alive && c.kind == KIND_PREY)
            .count() as u8
    }
    pub fn pred_n(&self) -> u8 {
        self.critters
            .iter()
            .filter(|c| c.alive && c.kind == KIND_PRED)
            .count() as u8
    }
    pub fn max_gen(&self) -> u8 {
        self.critters
            .iter()
            .filter(|c| c.alive)
            .map(|c| c.gen)
            .max()
            .unwrap_or(0)
    }
    pub fn food_n(&self) -> u8 {
        self.foods.iter().filter(|f| f.on).count() as u8
    }

    pub fn render(&self) -> [[u8; VW]; VH] {
        let mut fb = [[0u8; VW]; VH];
        let b = self.bright;
        for f in &self.foods {
            if !f.on {
                continue;
            }
            let x = wrapd(f.x - self.cam_x, WW).floor() as i32;
            let y = wrapd(f.y - self.cam_y, WH).floor() as i32;
            plot(
                &mut fb,
                x,
                y,
                ((f.a * b as f32 * 0.40).min(b as f32 * 0.42)) as u8,
            );
        }
        const FX: [i32; 8] = [1, 1, 0, -1, -1, -1, 0, 1];
        const FY: [i32; 8] = [0, -1, -1, -1, 0, 1, 1, 1];
        let mut trail = self.trail;
        for c in &self.critters {
            if !c.alive {
                continue;
            }
            let x = wrapd(c.x - self.cam_x, WW).round() as i32;
            let y = wrapd(c.y - self.cam_y, WH).round() as i32;
            let core =
                ((b as f32) * (0.45 + 0.55 * c.e.clamp(0.0, 1.0))).clamp(10.0, b as f32) as u8;
            if x >= 0 && y >= 0 && (x as usize) < VW && (y as usize) < VH {
                let dep = if c.kind == KIND_PRED {
                    18
                } else {
                    6 + ((c.g[5] as u16 * 28) / 255) as u8
                };
                let i = y as usize * VW + x as usize;
                if dep > trail[i] {
                    trail[i] = dep;
                }
            }
            let d = ((c.a / 0.785398).round() as i32) & 7;
            if c.kind == KIND_PRED {
                plot(&mut fb, x, y, core);
                plot(&mut fb, x + FX[d as usize], y + FY[d as usize], core);
                plot(&mut fb, x - FY[d as usize], y + FX[d as usize], core / 3);
                plot(&mut fb, x + FY[d as usize], y - FX[d as usize], core / 3);
            } else {
                plot(&mut fb, x, y, core);
                plot(&mut fb, x - FX[d as usize], y - FY[d as usize], core / 3);
            }
        }
        for y in 0..VH {
            for x in 0..VW {
                if trail[y * VW + x] > fb[y][x] {
                    fb[y][x] = trail[y * VW + x];
                }
            }
        }
        if self.shake > 0.2 {
            let rim = ((self.shake * b as f32 * 0.5).min(b as f32)) as u8;
            for x in 0..VW {
                if rim > fb[0][x] {
                    fb[0][x] = rim;
                }
                if rim > fb[VH - 1][x] {
                    fb[VH - 1][x] = rim;
                }
            }
        }
        fb
    }

    pub fn px_hex(&self) -> String {
        fb_hex(&self.render())
    }

    pub fn json(&self) -> String {
        format!(
            "{{\"ok\":true,\"sim\":\"tank\",\"w\":32,\"h\":9,\"alive\":{},\"prey\":{},\"pred\":{},\"births\":{},\"gen\":{},\"food\":{},\"follow\":{},\"shake\":{:.2},\"cam\":[{:.1},{:.1}],\"imu\":false,\"ldr\":false,\"source\":\"rust\"}}",
            self.alive(),
            self.prey_n(),
            self.pred_n(),
            self.births,
            self.max_gen(),
            self.food_n(),
            self.follow,
            self.shake,
            self.cam_x,
            self.cam_y
        )
    }
}

#[inline]
fn plot(fb: &mut [[u8; VW]; VH], x: i32, y: i32, c: u8) {
    if x >= 0 && y >= 0 && (x as usize) < VW && (y as usize) < VH && c > fb[y as usize][x as usize]
    {
        fb[y as usize][x as usize] = c;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn tank_lives() {
        let mut t = Tank::new(3);
        for _ in 0..800 {
            t.step();
        }
        assert!(t.alive() > 0);
        assert!(t.births > 8);
    }
}
