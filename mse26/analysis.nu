#!/usr/bin/env -S nu --

let data = open results.csv
  | where app != "ipamiric3ref" # App throws errors on many instances

let apps = $data | get app | uniq
let solvers = $data | columns | where $it not-in ["app" "input"]

let cols = ["app"] ++ $solvers

# Number of errors
let errors = $apps
| each {|app|
  $solvers
  | each {|slv| {$slv: (
    $data | where {|it| $it.app == $app and (($it | get $slv) == "error")} | length
  )}}
  | into record
  | insert app $app
} | append (
  $solvers
  | each {|slv| {$slv: (
    $data | where {|it| ($it | get $slv) == "error"} | length
  )}}
  | into record
  | insert app "all"
)
| select app ...$solvers

print "Number of errors"
print $errors

# Number of solved instances
let solved = $apps
| each {|app|
  $solvers
  | each {|slv| {$slv: (
    $data | where {|it| $it.app == $app and (($it | get $slv) not-in ["timeout" "memout" "error"])} | length
  )}}
  | into record
  | insert app $app
} | append (
  $solvers
  | each {|slv| {$slv: (
    $data | where {|it| ($it | get $slv) not-in ["timeout" "memout" "error"]} | length
  )}}
  | into record
  | insert app "all"
)
| select app ...$solvers

print "Number of solved instances"
print $solved

# PAR-2 scores
let par2 = $apps
| each {|app|
  $solvers
  | each {|slv|
    let dat = $data | where app == $app
    {$slv: (
      ($dat | get $slv | where $it in ["timeout" "memout" "error"] | length) * 3600 * 2
      + ($dat | get $slv | where $it not-in ["timeout" "memout" "error"] | append 0 | math sum)
    )}
  }
  | into record
  | insert app $app
} | append (
  $solvers
  | each {|slv|
    {$slv: (
      ($data | get $slv | where $it in ["timeout" "memout" "error"] | length) * 3600 * 2
      + ($data | get $slv | where $it not-in ["timeout" "memout" "error"] | append 0 | math sum)
    )}
  }
  | into record
  | insert app "all"
)
| select app ...$solvers

print "PAR-2 scores"
print $par2

let solvers_by_par2 = $par2
  | where app == "all"
  | select ...$solvers
  | transpose
  | rename -c {"column0": "solver", "column1": "par2"}
  | sort-by "par2"

# Legend for CDF plot
let legend = $solvers_by_par2
  | each {|it| $"($it.solver) \(par-2: ($it.par2 / 1000 | math round --precision 1)*10^3\)"}

let best_solver = $solvers_by_par2 | get "solver" | first

mut cdf_data = $data
  | get $best_solver
  | where $it not-in ["timeout" "memout" "error"]
  | append 0
  | sort
  | enumerate
  | rename -c {"item": $best_solver}

for slv in ($solvers_by_par2 | get "solver" | last (($solvers | length) - 1)) {
  $cdf_data = $cdf_data | merge (
    $data
    | get $slv
    | where $it not-in ["timeout" "memout" "error"]
    | append 0
    | sort
    | enumerate
    | rename -c {"item": $slv}
  )
}

$cdf_data | to csv --noheaders --separator ' ' | save -f cdf.dat

gnuplot -c cdf.plt ...$legend

let line_styles = $solvers_by_par2
  | get solver
  | enumerate
  | rename -c {"item": "solver"}

mut gnuplotvars = ""

for app in $apps {
  let solvers_by_app_par2 = $par2
    | where app == $app
    | select ...$solvers
    | transpose
    | rename -c {"column0": "solver", "column1": "par2"}
    | sort-by "par2"

  let best_solver = $solvers_by_app_par2 | get "solver" | first

  mut cdf_data = $data
    | where app == $app
    | get $best_solver
    | where $it not-in ["timeout" "memout" "error"]
    | append 0
    | sort
    | enumerate
    | rename -c {"item": $best_solver}

  for slv in ($solvers_by_app_par2 | get "solver" | last (($solvers | length) - 1)) {
    $cdf_data = $cdf_data | merge (
      $data
      | where app == $app
      | get $slv
      | where $it not-in ["timeout" "memout" "error"]
      | append 0
      | sort
      | enumerate
      | rename -c {"item": $slv}
    )
  }

  $cdf_data | to csv --noheaders --separator ' ' | save -f $"($app)-cdf.dat"

  $gnuplotvars = ($gnuplotvars
    ++ ";"
    ++ ($solvers_by_app_par2
      | enumerate
      | each {|it| $"($app)n($it.index + 1)=\"($it.item.solver) \(par-2: ($it.item.par2 / 1000 | math round --precision 1)*10^3\)\""}
      | str join ";")
    ++ ";"
    ++ ($solvers_by_app_par2
      | enumerate
      | each {|row| $"($app)l($row.index + 1)=(($line_styles | where solver == $row.item.solver | get index | first) + 1)"}
      | str join ";")
  )
}

gnuplot -c app-cdfs.plt $gnuplotvars
