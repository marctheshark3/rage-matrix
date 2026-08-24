use matrix_sim::{Tank, War};
use std::env;
use std::io::{self, Write};
use std::time::Instant;

fn usage() {
    eprintln!(
        "matrix-sim — host twin for LEDDisplay2 (firmware stays C++)\n\
         usage:\n\
           matrix-sim bench [steps]\n\
           matrix-sim war [--steps N] [--seed S] [--json]\n\
           matrix-sim tank [--steps N] [--seed S] [--json]\n\
           matrix-sim serve   # one-line JSON per stdin cmd: war|tank|step|seed|json"
    );
}

fn parse_flag(args: &[String], name: &str, default: u32) -> u32 {
    args.windows(2)
        .find(|w| w[0] == name)
        .and_then(|w| w[1].parse().ok())
        .unwrap_or(default)
}

fn bench(steps: u32) {
    let mut war = War::new(1);
    let mut tank = Tank::new(1);
    // warmup
    for _ in 0..200 {
        war.step();
        tank.step();
    }
    let t0 = Instant::now();
    for _ in 0..steps {
        war.step();
    }
    let war_ns = t0.elapsed().as_nanos() as f64 / steps as f64;
    let t1 = Instant::now();
    for _ in 0..steps {
        tank.step();
    }
    let tank_ns = t1.elapsed().as_nanos() as f64 / steps as f64;
    // include render (I²C-shaped cost on host)
    let t2 = Instant::now();
    for _ in 0..steps {
        war.step();
        let _ = war.render();
    }
    let war_r = t2.elapsed().as_nanos() as f64 / steps as f64;
    println!(
        "{{\"ok\":true,\"steps\":{},\"war_ns_per_step\":{:.1},\"tank_ns_per_step\":{:.1},\"war_step_render_ns\":{:.1},\"war_kills\":{},\"tank_alive\":{},\"war_matches\":{}}}",
        steps,
        war_ns,
        tank_ns,
        war_r,
        war.kill_w + war.kill_e,
        tank.alive(),
        war.match_n
    );
}

fn run_war(args: &[String]) {
    let steps = parse_flag(args, "--steps", 400);
    let seed = parse_flag(args, "--seed", 1);
    let json = args.iter().any(|a| a == "--json");
    let mut w = War::new(seed);
    for _ in 0..steps {
        w.step();
    }
    if json {
        println!("{}", w.json());
    } else {
        println!("{} px={}", w.json(), w.px_hex().len());
    }
}

fn run_tank(args: &[String]) {
    let steps = parse_flag(args, "--steps", 400);
    let seed = parse_flag(args, "--seed", 1);
    let json = args.iter().any(|a| a == "--json");
    let mut t = Tank::new(seed);
    for _ in 0..steps {
        t.step();
    }
    if json {
        println!("{}", t.json());
    } else {
        println!("{} px={}", t.json(), t.px_hex().len());
    }
}

fn serve() {
    let mut war = War::new(1);
    let mut tank = Tank::new(1);
    let mut which = "war";
    let stdin = io::stdin();
    let mut line = String::new();
    let mut out = io::stdout();
    loop {
        line.clear();
        if stdin.read_line(&mut line).ok().filter(|n| *n > 0).is_none() {
            break;
        }
        let raw = line.trim();
        if raw.is_empty() {
            continue;
        }
        let mut parts = raw.split_whitespace();
        let cmd = parts.next().unwrap_or("");
        match cmd {
            "war" => which = "war",
            "tank" => which = "tank",
            "step" => {
                if which == "war" {
                    war.step();
                } else {
                    tank.step();
                }
            }
            "seed" => {
                if which == "war" {
                    war.seed();
                } else {
                    tank.seed();
                }
            }
            "next" => war.rematch(),
            "west" => {
                let arty = parts.next() == Some("arty");
                war.reinforce(0, arty);
            }
            "east" => {
                let arty = parts.next() == Some("arty");
                war.reinforce(1, arty);
            }
            "wall" => {
                let x: i32 = parts.next().and_then(|s| s.parse().ok()).unwrap_or(16);
                let y: i32 = parts.next().and_then(|s| s.parse().ok()).unwrap_or(4);
                war.drop_wall(x, y);
            }
            "feed" => {
                let x = parts.next().and_then(|s| s.parse().ok());
                let y = parts.next().and_then(|s| s.parse().ok());
                tank.feed(x, y);
            }
            "scatter" => tank.scatter(),
            "shake" => {
                let amp: f32 = parts.next().and_then(|s| s.parse().ok()).unwrap_or(1.2);
                tank.shake(amp);
            }
            "hunt" => tank.drop_hunter(),
            "quit" | "exit" => break,
            _ => {}
        }
        let want_px = cmd == "px" || cmd == "jsonpx";
        if cmd == "px" {
            let s = if which == "war" {
                war.px_hex()
            } else {
                tank.px_hex()
            };
            writeln!(out, "{}", s).ok();
        } else if cmd == "jsonpx" {
            let st = if which == "war" {
                war.json()
            } else {
                tank.json()
            };
            let px = if which == "war" {
                war.px_hex()
            } else {
                tank.px_hex()
            };
            writeln!(out, "{}\t{}", st, px).ok();
        } else {
            let s = if which == "war" {
                war.json()
            } else {
                tank.json()
            };
            writeln!(out, "{}", s).ok();
        }
        let _ = want_px;
        out.flush().ok();
    }
}

fn main() {
    let args: Vec<String> = env::args().skip(1).collect();
    match args.first().map(String::as_str) {
        Some("bench") => bench(args.get(1).and_then(|s| s.parse().ok()).unwrap_or(50_000)),
        Some("war") => run_war(&args),
        Some("tank") => run_tank(&args),
        Some("serve") => serve(),
        _ => {
            usage();
            std::process::exit(2);
        }
    }
}
