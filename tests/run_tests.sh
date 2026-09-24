#!/usr/bin/env bash
# Compile (if needed) and run myc examples; check exit 0 and key output.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

if [[ ! -x ./myc ]]; then
  make
fi

pass=0
fail=0

run_expect() {
  local name="$1"
  local file="$2"
  local expect="$3"
  local out
  if ! out=$(./myc "$file" 2>&1); then
    echo "FAIL $name (nonzero exit)"
    echo "$out"
    fail=$((fail + 1))
    return
  fi
  if ! grep -qF -- "$expect" <<<"$out"; then
    echo "FAIL $name (missing '$expect')"
    echo "output was:"
    echo "$out"
    fail=$((fail + 1))
    return
  fi
  echo "PASS $name"
  pass=$((pass + 1))
}


run_error() {
  local name="$1"
  local file="$2"
  local expect_exit="$3"
  local expect_sub="$4"
  local out
  local ec=0
  out=$(./myc "$file" 2>&1) || ec=$?
  if [[ "$ec" -ne "$expect_exit" ]]; then
    echo "FAIL $name (exit $ec, expected $expect_exit)"
    echo "$out"
    fail=$((fail + 1))
    return
  fi
  if ! grep -qF -- "$expect_sub" <<<"$out"; then
    echo "FAIL $name (missing '$expect_sub' on stderr/stdout)"
    echo "output was:"
    echo "$out"
    fail=$((fail + 1))
    return
  fi
  echo "PASS $name"
  pass=$((pass + 1))
}

run_expect "hello"      examples/hello.my      "42"
run_expect "arithmetic" examples/arithmetic.my "13"
run_expect "fib"        examples/fib.my        "55"
run_expect "factorial"  examples/factorial.my  "720"
run_expect "fizzbuzz"   examples/fizzbuzz.my   "1500"

run_expect "types"      examples/types.my      "hello"
run_expect "types-float" examples/types.my     "3.14"
run_expect "list_map"   examples/list_map.my   "[1, 2, 3]"
run_expect "list_set"   examples/list_map.my   "9"
run_expect "map"        examples/list_map.my   '"a": 1'
run_expect "struct"     examples/struct.my     "Point {x: 1, y: 2}"
run_expect "struct_set" examples/struct.my     "10"
run_expect "loops_if"   examples/loops.my      "100"
run_expect "loops_for"  examples/loops.my      "22"
run_expect "loops_in"   examples/loops.my      "7"
run_expect "import"     examples/import_demo.my "42"
run_expect "import_str" examples/import_demo.my "hi myc"

# builtins/math
run_expect "math_pi"    examples/math_demo.my  "3.14159"
run_expect "math_sqrt"  examples/math_demo.my  "3.0"
run_expect "math_sin0"  examples/math_demo.my  "0.0"
run_expect "math_cos0"  examples/math_demo.my  "1.0"
run_expect "math_hypot" examples/math_demo.my  "5.0"
run_expect "math_pow"   examples/math_demo.my  "1024.0"

# builtins/http helpers (no port bind)
run_expect "http_method" examples/http_parse_test.my "GET"
run_expect "http_path"   examples/http_parse_test.my "/hello"
run_expect "http_proto"  examples/http_parse_test.my "HTTP/1.1 200 OK"
run_expect "http_html"   examples/http_parse_test.my "text/html"

# builtins/json (pure myc)
run_expect "json_stringify" examples/json_demo.my '"name":"myc"'
run_expect "json_roundtrip_name" examples/json_demo.my "myc"
run_expect "json_roundtrip_n" examples/json_demo.my "42"
run_expect "json_array_float" examples/json_demo.my "3.5"
run_expect "json_null" examples/json_demo.my "nil"
run_expect "json_escape" examples/json_demo.my 'a\"b'

# builtins/csv (pure myc + fs)
run_expect "csv_header" examples/csv_demo.my "name"
run_expect "csv_ada" examples/csv_demo.my "Ada"
run_expect "csv_quoted" examples/csv_demo.my 'Bo "Bob"'
run_expect "csv_maps_age" examples/csv_demo.my "36"
run_expect "csv_field_comma" examples/csv_demo.my "2,3"

# builtins/fs (C natives): write+read+remove in one script
run_expect "fs_hello" examples/fs_demo.my "hello"
run_expect "fs_more" examples/fs_demo.my "more"
run_expect "fs_gone" examples/fs_demo.my "false"

# builtins/os: argv + system/capture
run_expect "os_system0" examples/os_demo.my "0"
run_expect "os_capture" examples/os_demo.my "myc_ok"
run_expect "os_err"     examples/os_demo.my "err_msg"
run_expect "os_cwd"     examples/os_demo.my "string"

# argv: script name + user args (myc -d etc. stripped)
args_out=$(./myc examples/args_demo.my alpha beta 2>&1) || true
if grep -qF "alpha" <<<"$args_out" && grep -qF "beta" <<<"$args_out" \
   && grep -qF "args_demo.my" <<<"$args_out"; then
  echo "PASS args_demo"
  pass=$((pass + 1))
else
  echo "FAIL args_demo"
  echo "$args_out"
  fail=$((fail + 1))
fi

# -d before the script must not appear in os.args()
args_d=$(./myc -d examples/args_demo.my gamma 2>&1) || true
if grep -qF "gamma" <<<"$args_d" && grep -qF "args_demo.my" <<<"$args_d" \
   && ! grep -qxF -- "-d" <<<"$args_d"; then
  echo "PASS args_strip_flag"
  pass=$((pass + 1))
else
  echo "FAIL args_strip_flag"
  echo "$args_d"
  fail=$((fail + 1))
fi

# Disassemble path should still run and show opcodes
d_out=$(./myc -d examples/hello.my 2>&1)
if grep -q "OP_PRINT" <<<"$d_out" && grep -q "42" <<<"$d_out"; then
  echo "PASS disassemble"
  pass=$((pass + 1))
else
  echo "FAIL disassemble"
  echo "$d_out"
  fail=$((fail + 1))
fi

# Distinctive keywords still work (when/loop/ret/out)
run_expect "when_alias" examples/fib.my "55"

# builtins/graphics: available() only in automated tests (no run())
if [[ "$(uname -s)" = "Darwin" ]]; then
  run_expect "graphics_avail" examples/graphics_avail.my "true"
else
  run_expect "graphics_avail" examples/graphics_avail.my "false"
fi


# Error diagnostics (stderr + exit codes)
run_error "err_parse"   examples/errors/bad_parse.my  65 "error:"
run_error "err_parse_tok" examples/errors/bad_parse.my 65 "unexpected token"
run_error "err_undef"   examples/errors/undef_var.my  65 "undefined variable"
run_error "err_div0"    examples/errors/div_zero.my   70 "runtime error:"
run_error "err_div0_msg" examples/errors/div_zero.my  70 "division by zero"

echo "----"
echo "$pass passed, $fail failed"
[[ "$fail" -eq 0 ]]
