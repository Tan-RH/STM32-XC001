$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$output = Join-Path $env:TEMP "xc001_parser_tests.exe"

& gcc `
  -std=c11 -Wall -Wextra -Werror `
  "-I$projectRoot\Core\Inc" `
  "$PSScriptRoot\test_parsers.c" `
  "$projectRoot\Core\Src\xc001_config.c" `
  "$projectRoot\Core\Src\xc001_utils.c" `
  -o $output

if ($LASTEXITCODE -ne 0) {
  exit $LASTEXITCODE
}

& $output
exit $LASTEXITCODE
