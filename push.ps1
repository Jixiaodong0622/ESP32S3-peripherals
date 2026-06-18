<#
.SYNOPSIS
    一键上传项目到 GitHub
.DESCRIPTION
    自动执行 git add / commit / push 三步。
    被 .gitignore 忽略的文件（build/、sdkconfig 等）不会被上传。
.EXAMPLE
    .\push.ps1 "更新 README"
    .\push.ps1            # 不填说明时用当前时间作为提交信息
#>

param(
    [Parameter(Position = 0)]
    [string]$Message
)

# 切换到脚本所在目录，保证在项目根目录执行
Set-Location -Path $PSScriptRoot

# 没填提交说明就用时间戳
if ([string]::IsNullOrWhiteSpace($Message)) {
    $Message = "Update " + (Get-Date -Format "yyyy-MM-dd HH:mm")
}

Write-Host "==> 当前改动：" -ForegroundColor Cyan
git status --short

# 检查是否有改动
$changes = git status --porcelain
if ([string]::IsNullOrWhiteSpace($changes)) {
    Write-Host "没有需要提交的改动，已是最新。" -ForegroundColor Yellow
    exit 0
}

Write-Host "`n==> 暂存所有改动..." -ForegroundColor Cyan
git add -A

Write-Host "==> 提交：$Message" -ForegroundColor Cyan
git commit -m $Message
if (-not $?) {
    Write-Host "提交失败，已中止。" -ForegroundColor Red
    exit 1
}

Write-Host "==> 推送到 GitHub..." -ForegroundColor Cyan
git push
if ($?) {
    Write-Host "`n上传成功！" -ForegroundColor Green
} else {
    Write-Host "`n推送失败，请检查网络或先执行 git pull。" -ForegroundColor Red
    exit 1
}
