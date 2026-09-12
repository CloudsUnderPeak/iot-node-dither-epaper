param([int]$Port, [string]$TargetUrl, [int]$TimeoutSeconds = 90)
$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$deadline = [System.Threading.CancellationTokenSource]::new($TimeoutSeconds * 1000)
$client = [System.Net.WebSockets.ClientWebSocket]::new()
$script:sequence = 0
function Call-Cdp([string]$Method, $Parameters) {
    $script:sequence += 1
    $payload = @{id=$script:sequence;method=$Method;params=$Parameters} | ConvertTo-Json -Depth 30 -Compress
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($payload)
    $null = $client.SendAsync([System.ArraySegment[byte]]::new($bytes), [System.Net.WebSockets.WebSocketMessageType]::Text, $true, $deadline.Token).GetAwaiter().GetResult()
    do {
        $stream = [System.IO.MemoryStream]::new()
        do {
            $buffer = [byte[]]::new(65536)
            $received = $client.ReceiveAsync([System.ArraySegment[byte]]::new($buffer), $deadline.Token).GetAwaiter().GetResult()
            $stream.Write($buffer, 0, $received.Count)
        } while (!$received.EndOfMessage)
        $message = [System.Text.Encoding]::UTF8.GetString($stream.ToArray()) | ConvertFrom-Json
        $stream.Dispose()
    } while ($message.id -ne $script:sequence)
    if ($message.error) { throw ($message.error | ConvertTo-Json -Compress) }
    return $message.result
}
try {
    $pages = Invoke-RestMethod -Uri "http://127.0.0.1:$Port/json/list" -TimeoutSec $TimeoutSeconds
    $page = $pages | Where-Object { $_.type -eq 'page' } | Select-Object -First 1
    $null = $client.ConnectAsync([Uri]$page.webSocketDebuggerUrl, $deadline.Token).GetAwaiter().GetResult()
    $null = Call-Cdp 'Page.enable' @{}
    $null = Call-Cdp 'Page.navigate' @{url=$TargetUrl}
    # Navigation's new execution context is an explicit barrier, not a timed retry of a test.
    do {
        $state = Call-Cdp 'Runtime.evaluate' @{expression="document.URL !== 'about:blank' && document.readyState !== 'loading'";returnByValue=$true}
        if (!$state.result.value) { Start-Sleep -Milliseconds 20 }
    } while (!$state.result.value)
    $expression = Get-Content -Raw -Encoding UTF8 (Join-Path $PSScriptRoot 'browser-readiness.js')
    do {
        $result = Call-Cdp 'Runtime.evaluate' @{expression=[string]$expression;returnByValue=$true}
        if ($result.exceptionDetails) { throw ($result.exceptionDetails | ConvertTo-Json -Depth 20) }
        if (!$result.result.value) { Start-Sleep -Milliseconds 20 }
    } while (!$result.result.value)
    $dom = Call-Cdp 'Runtime.evaluate' @{expression='document.documentElement.outerHTML';returnByValue=$true}
    [Console]::Write($dom.result.value)
} finally {
    if ($client.State -eq [System.Net.WebSockets.WebSocketState]::Open) {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes('{"id":999999,"method":"Browser.close"}')
        $null = $client.SendAsync([System.ArraySegment[byte]]::new($bytes), [System.Net.WebSockets.WebSocketMessageType]::Text, $true, [System.Threading.CancellationToken]::None).GetAwaiter().GetResult()
    }
    $client.Dispose()
    $deadline.Dispose()
}
