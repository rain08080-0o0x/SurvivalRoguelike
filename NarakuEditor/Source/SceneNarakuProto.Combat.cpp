#include "SceneNarakuProtoImplementation.h"

using namespace SceneNarakuProtoImplementation;

NarakuCombat::FrameState SceneNarakuProto::PlayerCombatState() const
{
    const auto* attack = NarakuCombat::FindAttack(m_combatCatalog, m_player.activeAttackId);
    if (attack && m_player.attackTimer > 0) return NarakuCombat::EvaluateFrame(*attack, m_player.attackFrame);
    NarakuCombat::FrameState state;
    const auto* defaults = NarakuCombat::FindAttack(m_combatCatalog, "player.pickaxe.1");
    state.hurtbox = m_player.hasRetainedHurtbox ? m_player.retainedHurtbox :
        (defaults ? defaults->defaultHurtbox : NarakuCombat::HitboxKey{0, {0, 0.6f, 0}, {0.25f, 0.6f, 0.25f}});
    return state;
}

NarakuCombat::FrameState SceneNarakuProto::EnemyCombatState(const EnemyState& enemy) const
{
    const auto* attack = NarakuCombat::FindAttack(m_combatCatalog, enemy.type == EnemyType::Territory
        ? "enemy.territory.charge" : "enemy.charger.charge");
    if (attack && enemy.attackRunning) return NarakuCombat::EvaluateFrame(*attack, enemy.attackFrame);
    NarakuCombat::FrameState state;
    state.hurtbox = enemy.hasRetainedHurtbox ? enemy.retainedHurtbox :
        (attack ? attack->defaultHurtbox : NarakuCombat::HitboxKey{0, {0, 0.6f, 0}, {0.4f, 0.6f, 0.4f}});
    return state;
}

void SceneNarakuProto::UpdateEnemyAttack(EnemyState& enemy, float dt)
{
    const auto* attack = NarakuCombat::FindAttack(m_combatCatalog, enemy.activeAttackId);
    if (!attack || attack->framesPerSecond < 1) { enemy.attackRunning = false; return; }
    const float previous = enemy.attackFrame;
    enemy.attackFrame += dt * attack->framesPerSecond;
    const int total = NarakuCombat::PhaseFrames(*attack);
    const auto current = NarakuCombat::EvaluateFrame(*attack, enemy.attackFrame);
    enemy.retainedHurtbox = current.hurtbox; enemy.hasRetainedHurtbox = true;
    const float fps = static_cast<float>(attack->framesPerSecond);
    enemy.telegraphTimer = current.phaseIndex >= 0 && attack->phases[current.phaseIndex].type == NarakuCombat::PhaseType::Startup
        ? std::max(0.001f, (total - enemy.attackFrame) / fps) : 0.0f;
    enemy.chargeTimer = current.hasAttackBox ? std::max(0.001f, (total - enemy.attackFrame) / fps) : 0.0f;
    const bool canMove = attack->movementStartFrame >= 0 && enemy.attackFrame >= attack->movementStartFrame &&
        enemy.attackFrame < attack->movementEndFrame + 1;
    if (canMove)
    {
        enemy.pos = ResolveFloorMove(enemy.pos, Add(enemy.pos, Mul(enemy.chargeDir, enemy.moveSpeed * 3.0f * dt)),
            enemy.depth, enemy.type == EnemyType::Charger ? 0.4f : 0.5f, enemy.type == EnemyType::Charger ? 1.2f : 1.4f);
        enemy.facing = enemy.chargeDir; enemy.moving = true;
    }
    else if (attack->turnStartFrame >= 0 && enemy.attackFrame >= attack->turnStartFrame &&
        enemy.attackFrame < attack->turnEndFrame + 1)
    {
        enemy.chargeDir = Normalize(Sub(m_player.pos, enemy.pos)); enemy.facing = enemy.chargeDir;
    }
    const auto player = PlayerCombatState();
    const NarakuCombat::Vec3 enemyPosition = {enemy.pos.x, enemy.feetWorldY, enemy.pos.y};
    const NarakuCombat::Vec3 playerPosition = {m_player.pos.x, m_player.feetWorldY, m_player.pos.y};
    for (int frame = static_cast<int>(previous); frame <= std::min(total - 1, static_cast<int>(enemy.attackFrame)); ++frame)
    {
        const auto state = NarakuCombat::EvaluateFrame(*attack, static_cast<float>(frame));
        if (!state.hasAttackBox || player.invincible || m_player.stepTimer > kStepRecoveryTime ||
            (enemy.hasHitThisCharge && attack->singleHitPerTarget)) continue;
        if (!NarakuCombat::BoxesOverlap(state.attackBox, enemyPosition, std::atan2(enemy.facing.x, enemy.facing.y),
            player.hurtbox, playerPosition, std::atan2(m_player.facing.x, m_player.facing.y))) continue;
        const auto* playerAttack = NarakuCombat::FindAttack(m_combatCatalog, m_player.activeAttackId);
        const bool parried = m_player.attackTimer > 0 && playerAttack && playerAttack->parryStartFrame >= 0 &&
            m_player.attackFrame >= playerAttack->parryStartFrame && m_player.attackFrame < playerAttack->parryEndFrame + 1;
        if (!parried)
        {
            // 専用攻撃判定による命中だけがプレイヤーの攻撃を中断します。
            m_player.retainedHurtbox = player.hurtbox; m_player.hasRetainedHurtbox = true;
            ResetAttackCombo();
            ApplyPlayerDamage(enemy.attackDamage * attack->damageMultiplier, DeathCause::Enemy, u8"敵の攻撃で死亡しました。");
            m_cameraShakeTimer = kCameraShakeDuration;
            m_player.knockbackTimer = kKnockbackTime;
            m_player.knockbackVelocity = Mul(Normalize(Sub(m_player.pos, enemy.pos)), kKnockbackDistance / kKnockbackTime);
        }
        enemy.hasHitThisCharge = true;
        break;
    }
    if (enemy.attackFrame >= total)
    {
        enemy.attackRunning = false; enemy.telegraphTimer = 0; enemy.chargeTimer = 0;
        enemy.attackCooldown = enemy.attackInterval;
    }
}

SceneNarakuProto::CombatVisual* SceneNarakuProto::GetCombatVisual(const NarakuCombat::AttackDefinition& attack)
{
    if (attack.presentation != NarakuCombat::PresentationKind::ModelAnimation) return nullptr;
    for (auto& visual : m_combatVisuals) if (visual.id == attack.id) return &visual;
    CombatVisual visual; visual.id = attack.id;
    const auto* definition = NarakuCombat::FindModel(m_combatCatalog, attack.modelId);
    if (definition)
    {
        visual.model = new Model();
        const auto modelPath = WideToUtf8(ResolveRuntimeAssetPath(Utf8ToWide(definition->filePath)));
        if (!visual.model->Load(modelPath.c_str(), 1.0f, definition->zFlip ? Model::ZFlipUseAnime : Model::None))
            SAFE_DELETE(visual.model);
        if (visual.model)
        {
            const auto path = WideToUtf8(ResolveRuntimeAssetPath(Utf8ToWide(attack.animationPath.empty() ? definition->filePath : attack.animationPath)));
            visual.animation = visual.model->AddAnimation(path.c_str());
            const auto* animation = visual.model->GetAnimation(visual.animation);
            if (animation) { visual.seconds = animation->totalTime; visual.model->Play(visual.animation, false); }
        }
    }
    m_combatVisuals.push_back(visual);
    return &m_combatVisuals.back();
}

bool SceneNarakuProto::HasCombatVisual(const std::string& id, float frame)
{
    const auto* attack = NarakuCombat::FindAttack(m_combatCatalog, id);
    if (!attack) return false;
    const auto* visual = GetCombatVisual(*attack);
    return visual && visual->model && visual->animation >= 0 &&
        frame < std::max(static_cast<float>(attack->totalFrames), visual->seconds * attack->framesPerSecond);
}

void SceneNarakuProto::DrawCombatVisual(const std::string& id, float frame, const Vec2& facing,
    const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection)
{
    using namespace DirectX;
    const auto* attack = NarakuCombat::FindAttack(m_combatCatalog, id);
    if (!attack) return;
    auto* visual = GetCombatVisual(*attack);
    if (!visual || !visual->model || visual->animation < 0 ||
        frame >= std::max(static_cast<float>(attack->totalFrames), visual->seconds * attack->framesPerSecond)) return;
    const auto* definition = NarakuCombat::FindModel(m_combatCatalog, attack->modelId);
    visual->model->SetAnimationTime(visual->animation, frame / attack->framesPerSecond); visual->model->Step(0);
    const auto transform = NarakuCombat::EvaluateTransform(*attack, frame);
    const auto world = XMMatrixScaling(definition->scale * transform.scale.x, definition->scale * transform.scale.y, definition->scale * transform.scale.z) *
        XMMatrixRotationRollPitchYaw(XMConvertToRadians(transform.rotationDegrees.x), XMConvertToRadians(transform.rotationDegrees.y), XMConvertToRadians(transform.rotationDegrees.z)) *
        XMMatrixTranslation(transform.position.x, transform.position.y, transform.position.z) *
        XMMatrixRotationY(std::atan2(facing.x, facing.y)) * XMMatrixTranslation(position.x, position.y, position.z);
    XMFLOAT4X4 wvp[3]; XMStoreFloat4x4(&wvp[0], XMMatrixTranspose(world)); wvp[1] = view; wvp[2] = projection;
    ShaderList::SetWVP(wvp); ApplySceneLighting(); SetBlendMode(BLEND_NONE); SetCullingMode(D3D11_CULL_BACK);
    visual->model->SetVertexShader(ShaderList::GetVS(ShaderList::VS_ANIME));
    visual->model->SetPixelShader(ShaderList::GetPS(ShaderList::PS_LAMBERT));
    for (unsigned int i = 0; i < visual->model->GetMeshNum(); ++i)
    {
        const auto* mesh = visual->model->GetMesh(i);
        XMFLOAT4X4 bones[200];
        for (auto& bone : bones) XMStoreFloat4x4(&bone, XMMatrixIdentity());
        for (std::size_t j = 0; j < std::min<std::size_t>(200, mesh->bones.size()); ++j)
            XMStoreFloat4x4(&bones[j], XMMatrixTranspose(mesh->bones[j].invOffset * visual->model->GetBone(mesh->bones[j].index)));
        ShaderList::SetBones(bones);
        const auto* material = visual->model->GetMaterial(mesh->materialID);
        if (material) ShaderList::SetMaterial(*material);
        visual->model->Draw(static_cast<int>(i));
    }
}
