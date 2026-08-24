//! 48×18 bounded army. 32×9 is a camera. No heap in `step`.
use crate::{clamp, fb_hex, steer, Rng, VH, VW};

pub const WW: usize = 48;
pub const WH: usize = 18;
pub const MAX_U: usize = 16;
pub const MAX_S: usize = 14;
pub const TEAM_W: u8 = 0;
pub const TEAM_E: u8 = 1;

#[derive(Clone, Copy, Default)]
pub struct Unit {
    pub x: f32,
    pub y: f32,
    pub a: f32,
    pub hp: f32,
    pub g: [u8; 8],
    pub team: u8,
    pub gen: u8,
    pub cd: u8,
    pub alive: bool,
    pub role: u8, // 0 infantry, 1 arty
    pub kills: u8,
    pub dmg: u16,
}

#[derive(Clone, Copy, Default)]
pub struct Shot {
    pub x: f32,
    pub y: f32,
    pub dx: f32,
    pub dy: f32,
    pub team: u8,
    pub dmg: u8,
    pub splash: bool,
    pub on: bool,
    pub life: u8,
    pub arty: bool,
}

pub struct War {
    pub rng: Rng,
    pub cam_x: f32,
    pub cam_y: f32,
    pub match_n: u16,
    pub tick: u32,
    pub match_tick: u16,
    pub last_kill: u16,
    pub max_gen: u8,
    pub kill_w: u16,
    pub kill_e: u16,
    pub bright: u8,
    units: [Unit; MAX_U],
    shots: [Shot; MAX_S],
    wall: [u8; WW * WH],
    boom: [u8; VW * VH],
    elite: [[[u8; 8]; 4]; 2],
    elite_n: [u8; 2],
}

impl Default for War {
    fn default() -> Self {
        Self::new(1)
    }
}

impl War {
    pub fn new(seed: u32) -> Self {
        let mut w = Self {
            rng: Rng::new(seed),
            cam_x: 8.0,
            cam_y: 4.5,
            match_n: 0,
            tick: 0,
            match_tick: 0,
            last_kill: 0,
            max_gen: 0,
            kill_w: 0,
            kill_e: 0,
            bright: 90,
            units: [Unit::default(); MAX_U],
            shots: [Shot::default(); MAX_S],
            wall: [0; WW * WH],
            boom: [0; VW * VH],
            elite: [[[0; 8]; 4]; 2],
            elite_n: [0, 0],
        };
        w.seed();
        w
    }

    #[inline]
    fn wall_at(&self, x: i32, y: i32) -> u8 {
        if x < 0 || y < 0 || x >= WW as i32 || y >= WH as i32 {
            1
        } else {
            self.wall[y as usize * WW + x as usize]
        }
    }

    #[inline]
    fn blocked(&self, x: i32, y: i32) -> bool {
        self.wall_at(x, y) != 0
    }

    fn set_wall(&mut self, x: i32, y: i32, v: u8) {
        if x >= 0 && y >= 0 && x < WW as i32 && y < WH as i32 {
            self.wall[y as usize * WW + x as usize] = v;
        }
    }

    /// Grid DDA. Arty may skip one berm (1), never a bunker (2).
    fn los(&self, x0: f32, y0: f32, x1: f32, y1: f32, arty: bool) -> bool {
        let mut x = x0.floor() as i32;
        let mut y = y0.floor() as i32;
        let x1i = x1.floor() as i32;
        let y1i = y1.floor() as i32;
        if x == x1i && y == y1i {
            return true;
        }
        let dx = (x1i - x).abs();
        let dy = (y1i - y).abs();
        let sx = if x1i > x { 1 } else { -1 };
        let sy = if y1i > y { 1 } else { -1 };
        let mut err = dx - dy;
        let mut skipped = false;
        // skip origin cell
        loop {
            let e2 = err * 2;
            if e2 > -dy {
                err -= dy;
                x += sx;
            }
            if e2 < dx {
                err += dx;
                y += sy;
            }
            if x == x1i && y == y1i {
                return true;
            }
            let cell = self.wall_at(x, y);
            if cell == 0 {
                continue;
            }
            if cell == 2 {
                return false;
            }
            if arty && !skipped {
                skipped = true;
                continue;
            }
            return false;
        }
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

    fn rand_genome(&mut self) -> [u8; 8] {
        let mut g = [0u8; 8];
        for e in &mut g {
            *e = 40 + self.rng.range_u32(181) as u8;
        }
        g
    }

    fn build_map(&mut self) {
        self.wall = [0; WW * WH];
        for x in 0..WW as i32 {
            self.set_wall(x, 0, 1);
            self.set_wall(x, WH as i32 - 1, 1);
        }
        for y in 0..WH as i32 {
            self.set_wall(0, y, 1);
            self.set_wall(WW as i32 - 1, y, 1);
        }
        for _ in 0..3 {
            let x = 16 + self.rng.range_u32(16) as i32;
            let y = 2 + self.rng.range_u32((WH - 5) as u32) as i32;
            let len = 3 + self.rng.range_u32(5) as i32;
            let vert = self.rng.range_u32(2) == 1;
            for i in 0..len {
                let (xx, yy) = if vert { (x, y + i) } else { (x + i, y) };
                if xx > 2 && xx < WW as i32 - 3 && yy > 1 && yy < WH as i32 - 2 {
                    self.set_wall(xx, yy, 1);
                }
            }
        }
        let x = 18 + self.rng.range_u32(12) as i32;
        let y = 4 + self.rng.range_u32(8) as i32;
        for dy in 0..2 {
            for dx in 0..3 {
                if y + dy < WH as i32 - 2 && x + dx < WW as i32 - 3 {
                    self.set_wall(x + dx, y + dy, 2);
                }
            }
        }
    }

    fn slot(&self) -> Option<usize> {
        self.units.iter().position(|u| !u.alive)
    }

    fn spawn(&mut self, x: f32, y: f32, team: u8, mut role: u8, src: Option<[u8; 8]>, gen: u8) {
        let Some(i) = self.slot() else { return };
        let g = match src {
            Some(s) => self.mutate(s),
            None => self.rand_genome(),
        };
        if g[6] > 200 {
            role = 1;
        }
        self.units[i] = Unit {
            x: clamp(x, 1.0, WW as f32 - 2.0),
            y: clamp(y, 1.0, WH as f32 - 2.0),
            a: if team == TEAM_W {
                0.0
            } else {
                core::f32::consts::PI
            },
            hp: if role == 1 { 1.35 } else { 1.0 },
            g,
            team,
            gen,
            cd: self.rng.range_u32(13) as u8,
            alive: true,
            role,
            kills: 0,
            dmg: 0,
        };
    }

    fn deploy(&mut self, evolve: bool) {
        self.units = [Unit::default(); MAX_U];
        self.shots = [Shot::default(); MAX_S];
        self.boom = [0; VW * VH];
        self.match_tick = 0;
        self.last_kill = 0;
        self.match_n = self.match_n.saturating_add(1);
        for t in [TEAM_W, TEAM_E] {
            for i in 0..6 {
                let arty = i == 0;
                let x = if t == TEAM_W {
                    2.5 + self.rng.f01() * 6.0
                } else {
                    WW as f32 - 3.5 - self.rng.f01() * 6.0
                };
                let y = 2.0 + self.rng.f01() * (WH as f32 - 4.0);
                let (src, gen) = if evolve && self.elite_n[t as usize] > 0 {
                    let k = self.rng.range_u32(self.elite_n[t as usize] as u32) as usize;
                    (Some(self.elite[t as usize][k]), self.max_gen)
                } else {
                    (None, 0)
                };
                self.spawn(x, y, t, if arty { 1 } else { 0 }, src, gen);
            }
        }
        self.cam_x = 8.0;
        self.cam_y = 4.5;
    }

    fn harvest(&mut self) {
        for t in 0..2 {
            let mut scored: [(i32, usize); MAX_U] = [(i32::MIN, 0); MAX_U];
            let mut n = 0;
            for (i, u) in self.units.iter().enumerate() {
                if u.team != t as u8 {
                    continue;
                }
                if !u.alive && u.kills == 0 && u.dmg == 0 {
                    continue;
                }
                scored[n] = (u.kills as i32 * 80 + u.dmg as i32 + (u.hp * 20.0) as i32, i);
                n += 1;
            }
            scored[..n].sort_by(|a, b| b.0.cmp(&a.0));
            let take = n.min(4);
            self.elite_n[t] = take as u8;
            for k in 0..take {
                self.elite[t][k] = self.units[scored[k].1].g;
            }
        }
        self.max_gen = self.max_gen.saturating_add(1).min(250);
    }

    pub fn seed(&mut self) {
        self.elite_n = [0, 0];
        self.max_gen = 0;
        self.match_n = 0;
        self.kill_w = 0;
        self.kill_e = 0;
        self.tick = 0;
        self.build_map();
        self.deploy(false);
    }

    pub fn rematch(&mut self) {
        self.harvest();
        self.deploy(true);
    }

    pub fn n(&self, team: u8) -> u8 {
        self.units
            .iter()
            .filter(|u| u.alive && u.team == team)
            .count() as u8
    }

    pub fn drop_wall(&mut self, vx: i32, vy: i32) {
        let gx = clamp(self.cam_x.floor() + vx as f32, 2.0, WW as f32 - 3.0) as i32;
        let gy = clamp(self.cam_y.floor() + vy as f32, 2.0, WH as f32 - 3.0) as i32;
        for dy in -1..=1 {
            for dx in 0..3 {
                self.set_wall(gx + dx, gy + dy, if dx == 1 && dy == 0 { 2 } else { 1 });
            }
        }
    }

    pub fn reinforce(&mut self, team: u8, arty: bool) {
        let src = if self.elite_n[team as usize] > 0 {
            Some(self.elite[team as usize][0])
        } else {
            None
        };
        let x = if team == TEAM_W { 3.5 } else { WW as f32 - 4.0 };
        let y = 3.0 + self.rng.f01() * (WH as f32 - 6.0);
        self.spawn(x, y, team, if arty { 1 } else { 0 }, src, self.max_gen);
    }

    fn try_move(&mut self, i: usize, nx: f32, ny: f32) {
        let gx = nx.floor() as i32;
        let gy = ny.floor() as i32;
        if !self.blocked(gx, gy) {
            self.units[i].x = clamp(nx, 1.1, WW as f32 - 2.1);
            self.units[i].y = clamp(ny, 1.1, WH as f32 - 2.1);
            return;
        }
        let push = if self.units[i].team == TEAM_W {
            0.20
        } else {
            -0.20
        };
        let sx = self.units[i].x + push;
        if !self.blocked(sx.floor() as i32, self.units[i].y.floor() as i32) {
            self.units[i].x = clamp(sx, 1.1, WW as f32 - 2.1);
            return;
        }
        let sy = self.units[i].y + if (self.tick & 1) == 1 { 0.22 } else { -0.22 };
        if !self.blocked(self.units[i].x.floor() as i32, sy.floor() as i32) {
            self.units[i].y = clamp(sy, 1.1, WH as f32 - 2.1);
            return;
        }
        self.units[i].a += 1.15;
    }

    fn fire(&mut self, i: usize, tx: f32, ty: f32) {
        let Some(slot) = self.shots.iter().position(|s| !s.on) else {
            return;
        };
        let u = self.units[i];
        let dx = tx - u.x;
        let dy = ty - u.y;
        let d = (dx * dx + dy * dy).sqrt();
        if d < 0.2 {
            return;
        }
        let spd = if u.role == 1 { 0.55 } else { 0.70 };
        self.shots[slot] = Shot {
            x: u.x,
            y: u.y,
            dx: dx / d * spd,
            dy: dy / d * spd,
            team: u.team,
            dmg: if u.role == 1 { 22 } else { 14 },
            splash: u.role == 1,
            on: true,
            life: 28,
            arty: u.role == 1,
        };
        self.units[i].cd = if u.role == 1 {
            18 + (255 - u.g[3]) / 14
        } else {
            8 + (255 - u.g[3]) / 20
        };
    }

    fn nearest_enemy(&self, i: usize) -> Option<(usize, f32, f32, f32)> {
        let u = self.units[i];
        let mut best = None;
        let mut bd2 = f32::MAX;
        for (j, o) in self.units.iter().enumerate() {
            if !o.alive || o.team == u.team {
                continue;
            }
            let dx = o.x - u.x;
            let dy = o.y - u.y;
            let d2 = dx * dx + dy * dy;
            if d2 < bd2 {
                bd2 = d2;
                best = Some((j, dx, dy, d2));
            }
        }
        best
    }

    pub fn step(&mut self) {
        self.tick += 1;
        self.match_tick = self.match_tick.saturating_add(1);
        for b in &mut self.boom {
            *b = b.saturating_sub(8);
        }

        for i in 0..MAX_U {
            if !self.units[i].alive {
                continue;
            }
            if self.units[i].cd > 0 {
                self.units[i].cd -= 1;
            }
            let u = self.units[i];
            let speed = (if u.role == 1 { 0.07 } else { 0.16 })
                + (u.g[0] as f32 / 255.0) * (if u.role == 1 { 0.10 } else { 0.24 });
            let turn = 0.18 + (u.g[1] as f32 / 255.0) * 0.50;
            let range = (if u.role == 1 { 12.0 } else { 8.0 })
                + (u.g[2] as f32 / 255.0) * (if u.role == 1 { 14.0 } else { 8.0 });
            let acc = 0.72 + (u.g[4] as f32 / 255.0) * 0.26;
            let Some((ei, edx, edy, ed2)) = self.nearest_enemy(i) else {
                continue;
            };
            let can = self.los(u.x, u.y, self.units[ei].x, self.units[ei].y, u.role == 1);
            let stall = self.match_tick.saturating_sub(self.last_kill) > 180;
            let hold_x = if u.team == TEAM_W {
                11.0
            } else {
                WW as f32 - 12.0
            };
            if u.role == 1 {
                steer(
                    &mut self.units[i].a,
                    hold_x - u.x,
                    self.units[ei].y - u.y,
                    turn * 0.6,
                    1.0,
                );
                if (u.x - hold_x).abs() > 1.2 || !can {
                    let a = self.units[i].a;
                    self.try_move(i, u.x + a.cos() * speed, u.y + a.sin() * speed);
                }
            } else {
                let push = if u.team == TEAM_W { speed } else { -speed };
                steer(&mut self.units[i].a, edx, edy, turn, 1.0);
                let a = self.units[i].a;
                self.try_move(
                    i,
                    u.x + push * 0.75 + a.cos() * speed * 0.35,
                    u.y + a.sin() * speed,
                );
            }
            if self.units[i].cd == 0 && (can || stall) && ed2 < range * range {
                let j = (1.0 - acc) * 2.2;
                let tx = self.units[ei].x + (self.rng.f01() - 0.5) * j;
                let ty = self.units[ei].y + (self.rng.f01() - 0.5) * j;
                self.fire(i, tx, ty);
            }
        }

        for si in 0..MAX_S {
            if !self.shots[si].on {
                continue;
            }
            self.shots[si].x += self.shots[si].dx;
            self.shots[si].y += self.shots[si].dy;
            if self.shots[si].life > 0 {
                self.shots[si].life -= 1;
            }
            let gx = self.shots[si].x.floor() as i32;
            let gy = self.shots[si].y.floor() as i32;
            let cell = self.wall_at(gx, gy);
            let hit = cell != 0;
            let s = self.shots[si];
            if (hit && !s.arty)
                || s.life == 0
                || s.x < 0.0
                || s.y < 0.0
                || s.x >= WW as f32
                || s.y >= WH as f32
            {
                self.shots[si].on = false;
                continue;
            }
            if hit && s.arty && cell == 2 {
                self.shots[si].on = false;
                continue;
            }
            if hit && s.arty && cell == 1 {
                continue;
            }
            for j in 0..MAX_U {
                if !self.units[j].alive || self.units[j].team == s.team {
                    continue;
                }
                let dx = self.units[j].x - s.x;
                let dy = self.units[j].y - s.y;
                let rad = if s.splash { 2.1 } else { 0.95 };
                if dx * dx + dy * dy > rad * rad {
                    continue;
                }
                let armor = 0.55 + (self.units[j].g[5] as f32 / 255.0) * 0.40;
                let take = (s.dmg as f32 / 100.0) * (1.4 - armor);
                self.units[j].hp -= take;
                for k in 0..MAX_U {
                    if self.units[k].alive && self.units[k].team == s.team {
                        self.units[k].dmg = self.units[k].dmg.saturating_add((take * 40.0) as u16);
                    }
                }
                let vx = (s.x - self.cam_x).round() as i32;
                let vy = (s.y - self.cam_y).round() as i32;
                if vx >= 0 && vy >= 0 && vx < VW as i32 && vy < VH as i32 {
                    self.boom[vy as usize * VW + vx as usize] = 90;
                }
                if self.units[j].hp <= 0.0 {
                    self.units[j].alive = false;
                    if s.team == TEAM_W {
                        self.kill_w += 1;
                    } else {
                        self.kill_e += 1;
                    }
                    self.last_kill = self.match_tick;
                    if let Some(k) = self.units.iter().position(|u| u.alive && u.team == s.team) {
                        self.units[k].kills = self.units[k].kills.saturating_add(1);
                    }
                }
                self.shots[si].on = false;
                break;
            }
        }

        // camera on closest opposing pair
        let mut best = f32::MAX;
        let mut mx = 24.0;
        let mut my = 9.0;
        for i in 0..MAX_U {
            if !self.units[i].alive || self.units[i].team != TEAM_W {
                continue;
            }
            for j in 0..MAX_U {
                if !self.units[j].alive || self.units[j].team != TEAM_E {
                    continue;
                }
                let dx = self.units[j].x - self.units[i].x;
                let dy = self.units[j].y - self.units[i].y;
                let d2 = dx * dx + dy * dy;
                if d2 < best {
                    best = d2;
                    mx = (self.units[i].x + self.units[j].x) * 0.5;
                    my = (self.units[i].y + self.units[j].y) * 0.5;
                }
            }
        }
        let tx = clamp(mx - VW as f32 * 0.5, 0.0, WW as f32 - VW as f32);
        let ty = clamp(my - VH as f32 * 0.5, 0.0, WH as f32 - VH as f32);
        self.cam_x += (tx - self.cam_x) * 0.16;
        self.cam_y += (ty - self.cam_y) * 0.16;

        let stall = self.match_tick.saturating_sub(self.last_kill);
        if stall == 240 || stall == 400 {
            let lx = clamp(mx, 8.0, WW as f32 - 10.0) as i32;
            for y in 1..WH as i32 - 1 {
                self.set_wall(lx, y, 0);
                self.set_wall(lx + 1, y, 0);
                self.set_wall(lx + 2, y, 0);
            }
        }
        let nw = self.n(TEAM_W);
        let ne = self.n(TEAM_E);
        if (nw == 0 || ne == 0 || self.match_tick > 1800 || stall > 650) && self.match_tick > 80 {
            self.rematch();
        }
    }

    pub fn render(&self) -> [[u8; VW]; VH] {
        let mut fb = [[0u8; VW]; VH];
        let b = self.bright;
        let cx = self.cam_x.floor() as i32;
        let cy = self.cam_y.floor() as i32;
        for y in 0..VH as i32 {
            for x in 0..VW as i32 {
                let cell = self.wall_at(cx + x, cy + y);
                if cell != 0 {
                    fb[y as usize][x as usize] =
                        (b as f32 * if cell == 2 { 0.48 } else { 0.28 }) as u8;
                }
            }
        }
        for s in &self.shots {
            if !s.on {
                continue;
            }
            let x = (s.x - self.cam_x).round() as i32;
            let y = (s.y - self.cam_y).round() as i32;
            if x >= 0 && y >= 0 && (x as usize) < VW && (y as usize) < VH {
                fb[y as usize][x as usize] = b;
            }
        }
        for u in &self.units {
            if !u.alive {
                continue;
            }
            let x = (u.x - self.cam_x).round() as i32;
            let y = (u.y - self.cam_y).round() as i32;
            let hp = u.hp.clamp(0.0, 1.2);
            let core = ((b as f32) * (0.40 + 0.55 * hp)).clamp(12.0, b as f32) as u8;
            let mut plot = |xx: i32, yy: i32, c: u8| {
                if xx >= 0 && yy >= 0 && (xx as usize) < VW && (yy as usize) < VH {
                    let p = &mut fb[yy as usize][xx as usize];
                    if c > *p {
                        *p = c;
                    }
                }
            };
            if u.role == 1 {
                plot(x, y, core);
                plot(x + 1, y, core / 2);
                plot(x - 1, y, core / 2);
                plot(x, y + 1, core / 2);
                plot(x, y - 1, core / 2);
            } else if u.team == TEAM_W {
                plot(x, y, core);
                plot(x + 1, y, core / 3);
            } else {
                plot(x, y, core);
                plot(x - 1, y, core / 3);
            }
        }
        for y in 0..VH {
            for x in 0..VW {
                let boom = self.boom[y * VW + x].min(b);
                if boom > fb[y][x] {
                    fb[y][x] = boom;
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
            "{{\"ok\":true,\"sim\":\"war\",\"w\":32,\"h\":9,\"west\":{},\"east\":{},\"match\":{},\"gen\":{},\"kw\":{},\"ke\":{},\"tick\":{},\"cam\":[{:.1},{:.1}],\"imu\":false,\"ldr\":false,\"source\":\"rust\"}}",
            self.n(TEAM_W),
            self.n(TEAM_E),
            self.match_n,
            self.max_gen,
            self.kill_w,
            self.kill_e,
            self.match_tick,
            self.cam_x,
            self.cam_y
        )
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn campaign_kills() {
        let mut w = War::new(7);
        for _ in 0..4000 {
            w.step();
        }
        assert!(w.kill_w + w.kill_e > 0, "expected contact");
        assert!(w.match_n >= 1);
    }
}
