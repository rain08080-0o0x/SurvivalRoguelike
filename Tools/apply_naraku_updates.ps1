$root = Split-Path -Parent $PSScriptRoot
$cppPath = Join-Path $root "NarakuGame\Source\SceneNarakuProto.cpp"
$lines = [System.IO.File]::ReadAllLines($cppPath, [System.Text.Encoding]::UTF8)

# 行ベースで置換
for ($i = 0; $i -lt $lines.Length; $i++) {
    $line = $lines[$i]

    # WASD
    if ($line.Contains("if (!isMining && IsKeyPress('W')) input.y += 1.0f;")) {
        $lines[$i] = "    if (!isMining) { GetActionMoveVector(input.x, input.y); }"
        $lines[$i+1] = ""
        $lines[$i+2] = ""
        $lines[$i+3] = ""
        $lines[$i+4] = ""
        $lines[$i+5] = ""
        $lines[$i+6] = ""
        $lines[$i+7] = ""
        $lines[$i+8] = ""
        $lines[$i+9] = ""
        Write-Host "Replaced WASD at line $i"
    }

    # Jump
    if ($line.Contains("if (!isMining && !inLandingRecovery && IsKeyTrigger(VK_SPACE)) TryStartJump();")) {
        $lines[$i] = "    if (!isMining && !inLandingRecovery && IsActionJumpTrigger()) TryStartJump();"
        Write-Host "Replaced Jump at line $i"
    }

    # Interact
    if ($line.Trim() -eq "if (IsKeyTrigger('F')) TryInteract();") {
        $lines[$i] = "    if (IsActionInteractTrigger()) TryInteract();"
        Write-Host "Replaced Interact at line $i"
    }
    if ($line.Trim() -eq "if (IsKeyTrigger('F')) TryInteractSurface();") {
        $lines[$i] = "    if (IsActionInteractTrigger()) TryInteractSurface();"
        Write-Host "Replaced InteractSurface at line $i"
    }

    # Attack trigger
    if ($line.Contains("if (IsMouseLeftTrigger()) TryStartAttack();")) {
        $lines[$i] = '        if (IsActionAttackTrigger()) { if (GetLastActiveDevice() == ActiveInputDevice::KeyboardMouse) UpdateAimDirectionFromMouse(); TryStartAttack(); }'
        Write-Host "Replaced Attack at line $i"
    }

    # Unknown weapon attack
    if ($line.Trim() -eq "if (!IsMouseLeftPress())") {
        $lines[$i] = "    if (!IsActionAttackPress())"
        Write-Host "Replaced Unknown Weapon at line $i"
    }

    # 60 degree attack cone
    if ($line.Contains("Dot(Normalize(toEnemy), m_player.facing) > 0.25f")) {
        $lines[$i] = $line.Replace("Dot(Normalize(toEnemy), m_player.facing) > 0.25f", "Dot(Normalize(toEnemy), m_player.facing) >= 0.866025f")
        Write-Host "Replaced 60 degree attack cone at line $i"
    }

    # Skill
    if ($line.Trim() -eq "const bool pressed = IsKeyPress('Q');") {
        $lines[$i] = "    const bool pressed = IsActionSkillPress();"
        Write-Host "Replaced Skill at line $i"
    }

    # Shift
    if ($line.Trim() -eq "bool SceneNarakuProto::IsShiftPress() const") {
        $lines[$i+2] = "    if (IsActionDashStepPress()) return true;"
        Write-Host "Updated IsShiftPress at line $i"
    }

    # Menu draw branch (5543)
    if ($line.Contains("if (m_inventoryMapShowingMap) DrawMapControls();") -and $lines[$i-1].Contains("if (m_mode == Mode::Inventory)")) {
        $lines[$i] = '        if (m_activeMenuTab == MenuTab::Map) DrawMapControls(); else if (m_activeMenuTab == MenuTab::Settings) { ImGui::SetNextWindowPos(ImVec2(160.0f, 80.0f), ImGuiCond_FirstUseEver); ImGui::SetNextWindowSize(ImVec2(720.0f, 600.0f), ImGuiCond_FirstUseEver); if (ImGui::Begin(u8"設定", nullptr, ImGuiWindowFlags_NoCollapse)) { if (ImGui::Button(u8"← [LB] 地図", ImVec2(140.0f, 28.0f))) { m_activeMenuTab = MenuTab::Map; m_inventoryMapShowingMap = true; } ImGui::SameLine(); if (ImGui::Button(u8"所持品", ImVec2(140.0f, 28.0f))) { m_activeMenuTab = MenuTab::Inventory; m_inventoryMapShowingMap = false; } ImGui::SameLine(); ImGui::TextColored(ImVec4(0.3f, 0.9f, 1.0f, 1.0f), u8"【 設定 】 [RB] →"); ImGui::Separator(); m_inputSettings.DrawSettingsTab(); } ImGui::End(); } else DrawInventory();'
        $lines[$i+1] = ""
        Write-Host "Replaced Menu draw at line $i"
    }

    # Camera controls
    if ($line.Contains("constexpr float mouseSensitivity = 0.006f;")) {
        $lines[$i] = '    float yawDelta = 0.0f, pitchDelta = 0.0f; GetActionCameraRotation(yawDelta, pitchDelta); m_cameraYaw += yawDelta; m_cameraPitch += pitchDelta; const float zoomDelta = GetActionCameraZoomDelta(); if (zoomDelta != 0.0f) m_cameraDistance -= zoomDelta;'
        Write-Host "Replaced Camera at line $i"
    }
}

# 末尾にマウスエイム関数を追加
$newContent = [string]::Join("`r`n", $lines)
if (!$newContent.Contains("GetMouseAimGroundPosition")) {
    $mouseAim = '

SceneNarakuProto::Vec2 SceneNarakuProto::GetMouseAimGroundPosition()
{
    const POINT mousePos = GetMousePosition();
    HWND hWnd = GetActiveWindow();
    if (!hWnd) hWnd = GetForegroundWindow();
    RECT clientRect{};
    if (hWnd) GetClientRect(hWnd, &clientRect);
    const float screenW = static_cast<float>(std::max(1L, clientRect.right - clientRect.left));
    const float screenH = static_cast<float>(std::max(1L, clientRect.bottom - clientRect.top));
    const float ndcX = (static_cast<float>(mousePos.x) / screenW - 0.5f) * 2.0f;
    const float ndcY = (static_cast<float>(mousePos.y) / screenH - 0.5f) * -2.0f;
    const Vec2 camForward = GetCameraForward();
    const Vec2 camRight = GetCameraRight();
    const float aimRange = kAttackRange * 2.0f;
    return Add(m_player.pos, Add(Mul(camRight, ndcX * aimRange), Mul(camForward, ndcY * aimRange)));
}

void SceneNarakuProto::UpdateAimDirectionFromMouse()
{
    const Vec2 targetGround = GetMouseAimGroundPosition();
    const Vec2 toTarget = Sub(targetGround, m_player.pos);
    if (Distance(targetGround, m_player.pos) > 0.05f)
    {
        m_player.facing = Normalize(toTarget);
    }
}
'
    $newContent = $newContent + $mouseAim
    Write-Host "Appended Mouse Aim"
}

[System.IO.File]::WriteAllText($cppPath, $newContent, [System.Text.Encoding]::UTF8)
Write-Host "All done!"
