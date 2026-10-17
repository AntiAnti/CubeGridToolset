param([Parameter(Mandatory)][string]$RequestPath, [Parameter(Mandatory)][string]$ResponsePath)
$ErrorActionPreference = 'Stop'
$workspaceRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..'))
foreach ($taskPath in @($RequestPath, $ResponsePath)) {
    $resolved = [IO.Path]::GetFullPath($taskPath)
    if (-not $resolved.StartsWith($workspaceRoot + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Request and response files must be inside the project workspace.'
    }
}
$endpoint = 'http://127.0.0.1:8000/mcp'
$headers = @{ Accept = 'application/json, text/event-stream' }
$initialize = '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-11-25","capabilities":{},"clientInfo":{"name":"CubeGridToolset-workflow","version":"1.0"}}}'
$reply = Invoke-WebRequest -UseBasicParsing -TimeoutSec 15 -Uri $endpoint -Method Post -ContentType 'application/json' -Headers $headers -Body $initialize
if ($reply.Headers['Mcp-Session-Id']) { $headers['Mcp-Session-Id'] = $reply.Headers['Mcp-Session-Id'] }
$headers['MCP-Protocol-Version'] = '2025-11-25'
$requests = Get-Content -Raw -Encoding UTF8 -LiteralPath $RequestPath | ConvertFrom-Json
$responses = @()
foreach ($request in $requests) {
    if ($request.method -ne 'tools/call' -and $request.method -ne 'tools/list') {
        throw 'Only MCP tool requests are supported by this helper.'
    }
    $payload = $request | ConvertTo-Json -Depth 70 -Compress
    $reply = Invoke-WebRequest -UseBasicParsing -TimeoutSec 90 -Uri $endpoint -Method Post -ContentType 'application/json; charset=utf-8' -Headers $headers -Body ([Text.Encoding]::UTF8.GetBytes($payload))
    $body = $reply.Content
    if ($body.StartsWith('event:') -or $body.StartsWith('data:')) {
        $dataLines = @($body -split "`n" | Where-Object { $_.StartsWith('data:') })
        $body = ($dataLines | ForEach-Object { $_.Substring(5).Trim() }) -join "`n"
    }
    $response = $body | ConvertFrom-Json
    $responses += $response
    if ($response.error -or $response.result.isError) { break }
}
ConvertTo-Json -InputObject @($responses) -Depth 80 | Set-Content -Encoding UTF8 -LiteralPath $ResponsePath
Write-Output ('Saved ' + $responses.Count + ' MCP responses to ' + $ResponsePath)
