use latency_atlas::Histogram;
use std::io::{self, BufRead, Read, Write};

fn run() -> Result<(), String> {
    let args: Vec<String> = std::env::args().skip(1).collect();
    if args == ["--help"] {
        println!("latency-atlas [--json]\nRead one unsigned integer nanosecond sample per line from stdin.\nEmpty lines are ignored. Output quantile bounds; invalid input exits 2.");
        return Ok(());
    }
    if !args.is_empty() && args != ["--json"] {
        return Err("usage: latency-atlas [--json]".into());
    }
    let mut h = Histogram::default();
    let mut input = io::stdin().lock();
    let mut line = Vec::with_capacity(64);
    let mut number = 0_u64;
    loop {
        line.clear();
        // Limit each read even for a malicious line without a newline.
        let read = std::io::Read::by_ref(&mut input)
            .take(66)
            .read_until(b'\n', &mut line)
            .map_err(|e| e.to_string())?;
        if read == 0 {
            break;
        }
        number += 1;
        if read > 64 {
            return Err(format!("line {number}: exceeds 64 bytes"));
        }
        let text = std::str::from_utf8(&line)
            .map_err(|_| format!("line {number}: not UTF-8"))?
            .trim_ascii();
        if text.is_empty() {
            continue;
        }
        if !text.bytes().all(|b| b.is_ascii_digit()) {
            return Err(format!(
                "line {number}: expected unsigned integer nanoseconds"
            ));
        }
        let value = text
            .parse::<u64>()
            .map_err(|_| format!("line {number}: outside u64 range"))?;
        h.record(value)
            .map_err(|_| "sample count overflow".to_string())?;
    }
    let mut out = io::BufWriter::new(io::stdout().lock());
    if args == ["--json"] {
        write!(
            out,
            "{{\"count\":{},\"min\":{},\"max\":{},\"quantiles\":{{",
            h.count(),
            h.min().map_or("null".into(), |v| v.to_string()),
            h.max().map_or("null".into(), |v| v.to_string())
        )
        .map_err(|e| e.to_string())?;
        for (i, (name, p)) in [("p50", 5000), ("p95", 9500), ("p99", 9900)]
            .iter()
            .enumerate()
        {
            if i != 0 {
                write!(out, ",").map_err(|e| e.to_string())?;
            }
            let value = h
                .quantile(*p)
                .unwrap()
                .map_or("null".into(), |b| format!("[{},{}]", b.lower, b.upper));
            write!(out, "\"{name}\":{value}").map_err(|e| e.to_string())?;
        }
        writeln!(out, "}}}}").map_err(|e| e.to_string())?;
    } else {
        writeln!(
            out,
            "LATENCY ATLAS\n{} samples · nanoseconds\nPercentiles are inclusive bucket bounds.",
            h.count()
        )
        .map_err(|e| e.to_string())?;
        for (name, p) in [("p50", 5000), ("p95", 9500), ("p99", 9900)] {
            let value = h.quantile(p).unwrap().map_or("no samples".into(), |b| {
                format!("{} .. {} ns", b.lower, b.upper)
            });
            writeln!(out, "{name}  {value}").map_err(|e| e.to_string())?;
        }
    }
    out.flush().map_err(|e| e.to_string())
}

fn main() {
    if let Err(error) = run() {
        eprintln!("latency-atlas: {error}");
        std::process::exit(2);
    }
}
