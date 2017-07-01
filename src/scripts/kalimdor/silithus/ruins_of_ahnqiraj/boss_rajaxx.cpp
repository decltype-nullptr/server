/* Copyright (C) 2006 - 2010 ScriptDev2 <https://scriptdev2.svn.sourceforge.net/>
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* ScriptData
SDName: Boss_Rajaxx
SD%Complete: 90
SDComment: Sometimes event bugs appear
SDCategory: Ruins of Ahn'Qiraj
EndScriptData */

#include "ruins_of_ahnqiraj.h"
#include "scriptPCH.h"

#define GOSSIP_START "Let's find out."

enum
{
    SAY_ANDOROV_READY  = -1509003,
    SAY_ANDOROV_INTRO2 = -1509029,
    SAY_ANDOROV_ATTACK = -1509030,
    SAY_ANDOROV_INTRO  = -1509004,

    SAY_WAVE3 = -1509005,
    SAY_WAVE4 = -1509006,
    SAY_WAVE5 = -1509007,
    SAY_WAVE6 = -1509008,
    SAY_WAVE7 = -1509009,
    SAY_WAVE8 = -1509010,

    SAY_UNK1  = -1509011,
    SAY_UNK2  = -1509012,
    SAY_UNK3  = -1509013,
    SAY_DEATH = -1509014,

    SAY_DEAGGRO       = -1509015,
    SAY_KILLS_ANDOROV = -1509016,

    SAY_AQ_WAR_START = -1509017, // Yell when realm complete quest 8743 for world event
    EMOTE_FRENZY     = -1000001,

    // General Rajaxx
    SPELL_DISARM        = 6713,
    SPELL_FRENZY        = 8269,
    SPELL_SUMMON_PLAYER = 20477,
    SPELL_THUNDERCRASH  = 25599,
    SPELL_TRASH         = 3391,

    SPELL_CHARGE  = 26561,
    SPELL_REFLECT = 9906,
    SPELL_FEAR    = 19408,
    SPELL_ENRAGE  = 28747,

    // NPC General Andorov
    SPELL_AURA_OF_COMMAND = 25516,
    SPELL_BASH            = 25515,
    SPELL_STRIKE          = 22591,

    ZONE_SILITHUS = 1377,
};

#ifdef DEBUG_MODE
#define DEBUG_EMOTE_YELL(crea, texte) crea->MonsterYell(texte, 0)
#define DELAY_BETWEEN_WAVE 30000
#else
#define DEBUG_EMOTE_YELL(crea, texte)
#define DELAY_BETWEEN_WAVE 180000
#endif

#define ANDOROV_WAYPOINT_MAX 7
#define OOC_BETWEEN_WAVE 1000

struct RespawnAndEvadeHelper
{
    explicit RespawnAndEvadeHelper(Creature* _pCreature)
        : pCreature(_pCreature)
    {
    }
    void operator()() const
    {
        if (!pCreature->isAlive())
            pCreature->Respawn();
        pCreature->AI()->EnterEvadeMode();
    }
    Creature* pCreature;
};

struct boss_rajaxxAI : public ScriptedAI
{
    boss_rajaxxAI(Creature* pCreature)
        : ScriptedAI(pCreature)
    {
        m_pInstance = (ScriptedInstance*)pCreature->GetInstanceData();
        Reset();
    }

    ScriptedInstance* m_pInstance;

    uint32 m_uiResetAggro_Timer;
    uint32 m_uiThunderCrash_Timer;
    uint32 m_uiTrash_Timer;
    uint32 m_uiDisarm_Timer;
    uint32 m_uiWave_Timer;
    uint32 m_uiNextWave_Timer;
    uint32 m_uiNextWaveIndex;
    bool m_bHasEnraged;

    void Reset()
    {
        // Rajaxx's spells
        m_uiResetAggro_Timer   = 20000;
        m_uiThunderCrash_Timer = 25000;
        m_uiDisarm_Timer       = 5000;
        m_bHasEnraged          = false;
        m_uiTrash_Timer        = 30000;

        // Waves reset
        m_uiWave_Timer     = 1000;
        m_uiNextWave_Timer = 0;
        m_uiNextWaveIndex  = 0;

        if (m_pInstance)
        {
            for (uint8 waveIndex = 0; waveIndex < WAVE_MAX; ++waveIndex)
                ResetWave(waveIndex);
            m_pInstance->SetData(TYPE_RAJAXX, NOT_STARTED);
        }
    }

    void ResetWave(uint8 waveIndex)
    {
        if (!m_pInstance)
            return;
        if (waveIndex >= WAVE_MAX)
            return;

        DEBUG_EMOTE_YELL(m_creature, "DEBUG : Wave Reset");
        uint64 leaderGUID = GetLeaderGuidFromWaveIndex(waveIndex);
        if (Creature* pLeader = m_pInstance->GetCreature(leaderGUID))
            if (CreatureGroup* group = pLeader->GetCreatureGroup())
                group->RespawnAll(m_creature);
    }

    void StartWave(uint8 waveIndex)
    {
        if (!m_pInstance)
            return;

        DEBUG_EMOTE_YELL(m_creature, "DEBUG : StartWave");
        if (waveIndex < WAVE_MAX)
        {
            for (uint8 waveInd = 0; waveInd <= waveIndex; ++waveInd)
            {
                uint64 leaderGUID = GetLeaderGuidFromWaveIndex(waveInd);
                if (Creature* pLeader = m_pInstance->GetCreature(leaderGUID))
                {
                    if (pLeader->isAlive())
                        pLeader->SetInCombatWithZone();
                }
            }

            switch (waveIndex)
            {
                case 2: DoScriptText(SAY_WAVE3, m_creature); break;
                case 3: DoScriptText(SAY_WAVE4, m_creature); break;
                case 4: DoScriptText(SAY_WAVE5, m_creature); break;
                case 5: DoScriptText(SAY_WAVE6, m_creature); break;
                case 6: DoScriptText(SAY_WAVE7, m_creature); break;
            }
        }
    }

    uint64 GetLeaderGuidFromWaveIndex(uint8 waveIndex)
    {
        uint32 data;

        if (!m_pInstance)
            return 0;

        switch (waveIndex)
        {
            case 0: data = DATA_QEEZ; break;
            case 1: data = DATA_TUUBID; break;
            case 2: data = DATA_DRENN; break;
            case 3: data = DATA_XURREM; break;
            case 4: data = DATA_YEGGETH; break;
            case 5: data = DATA_PAKKON; break;
            case 6: data = DATA_ZERRAN; break;
            default: return 0;
        }
        return m_pInstance->GetData64(data);
    }

    void Aggro(Unit* pPuller)
    {
        m_creature->SetInCombatWithZone();

        if (m_pInstance)
            m_pInstance->SetData(TYPE_RAJAXX, IN_PROGRESS);
    }

    void JustDied(Unit* pKiller)
    {
        if (!m_pInstance)
            return;

        DoScriptText(SAY_DEATH, m_creature);
        if (m_pInstance)
            m_pInstance->SetData(TYPE_RAJAXX, DONE);

        // According to http://wowwiki.wikia.com/wiki/Cenarion_Circle_reputation_guide
        // and http://wowwiki.wikia.com/wiki/General_Rajaxx,
        // When Rajaxx dies, players should gain 90 (post-"nerf" 150) reputation for each
        // of the NPCs that are still alive.
        OnKillReputationReward();
    }

    void KilledUnit(Unit* pKilled)
    {
        //        if (!m_creature->isInCombat())
        //            DoScriptText(SAY_DEAGGRO, m_creature, pKilled);
    }

    void DamageTaken(Unit* pDoneBy, uint32& uiDamage)
    {
        // Frenzy
        if (!m_bHasEnraged && ((m_creature->GetHealth() * 100) / m_creature->GetMaxHealth()) < 30)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_FRENZY) == CAST_OK)
            {
                DoScriptText(EMOTE_FRENZY, m_creature);
                m_bHasEnraged = true;
            }
        }
    }

    bool IsCurrentWaveDead()
    {
        if (!m_pInstance || (m_uiNextWaveIndex == 0))
            return false;

        // Count of dead mobs per wave is managed by instance script
        if (m_pInstance->GetData(m_uiNextWaveIndex - 1 + WAVE_OFFSET) == 0)
            return true;

        return false;
    }

    void OnKillReputationReward()
    {
        FactionEntry const* factionEntry = sFactionStore.LookupEntry(609); // Cenarion Circle
        if (!factionEntry)
        {
            sLog.outError("Rajaxx justDied, unable to find Cenarion Circle faction");
            return;
        }
        std::list<Creature*> helpers;
        GetCreatureListWithEntryInGrid(helpers, m_creature, {15473, 15478, 15471, 987001}, 400.0f);

        if (!helpers.size())
            return;
        int alive = 0;
        for (auto it : helpers)
            if (it->isAlive())
                ++alive;

        if (Player* pLootRecepient = m_creature->GetLootRecipient())
        {
            if (Group* pGroup = pLootRecepient->GetGroup())
            {
                for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
                {
                    Player* pGroupGuy = itr->getSource();
                    if (!pGroupGuy || !pGroupGuy->IsInWorld())
                        continue;

                    uint32 current_reputation_rank1 =
                        pGroupGuy->GetReputationMgr().GetRank(factionEntry);
                    if (factionEntry && current_reputation_rank1 <= 7)
                    {
                        for (int i = 0; i < alive; i++)
                            pGroupGuy->GetReputationMgr().ModifyReputation(factionEntry, 90);
                    }
                }
            }
        }
    }

    void UpdateAI(const uint32 uiDiff)
    {
        // Waves launcher
        if (m_pInstance && (m_pInstance->GetData(TYPE_RAJAXX) == IN_PROGRESS))
        {
            if (IsCurrentWaveDead())
                m_uiNextWave_Timer += uiDiff;
            else
                m_uiNextWave_Timer = 0;

            if (m_uiNextWaveIndex < WAVE_MAX)
            {
                if ((m_uiWave_Timer < uiDiff) || (m_uiNextWave_Timer > OOC_BETWEEN_WAVE))
                {
                    StartWave(m_uiNextWaveIndex);
                    m_uiNextWaveIndex++;
                    m_uiWave_Timer = DELAY_BETWEEN_WAVE;
                }
                else
                    m_uiWave_Timer -= uiDiff;
            }
            else if (m_uiNextWaveIndex == WAVE_MAX) // Rajaxx
            {
                if (IsCurrentWaveDead())
                {
                    DoScriptText(SAY_WAVE8, m_creature);
                    m_creature->SetInCombatWithZone();
                    m_uiNextWaveIndex++;
                }
            }
        }

        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        // ThunderCrash
        if (m_uiThunderCrash_Timer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_THUNDERCRASH) == CAST_OK)
            {
                m_uiThunderCrash_Timer = urand(18000, 22000);
                m_creature->getThreatManager().modifyThreatPercent(m_creature->getVictim(), -100);
                if (Unit* victim = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0))
                    m_creature->AI()->AttackStart(victim);
            }
        }
        else
            m_uiThunderCrash_Timer -= uiDiff;

        // ResetAggro
        if (m_uiResetAggro_Timer < uiDiff)
        {
            m_uiResetAggro_Timer = urand(15000, 17000);
            DoScriptText(SAY_DEAGGRO, m_creature, m_creature->getVictim());
            m_creature->getThreatManager().modifyThreatPercent(m_creature->getVictim(), -100);
        }
        else
            m_uiResetAggro_Timer -= uiDiff;

        /* Trash */
        if (m_uiTrash_Timer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_TRASH) == CAST_OK)
                m_uiTrash_Timer = urand(19000, 23000);
        }
        else
            m_uiTrash_Timer -= uiDiff;

        // Disarm
        if (m_uiDisarm_Timer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_DISARM) == CAST_OK)
                m_uiDisarm_Timer = 15000;
        }
        else
            m_uiDisarm_Timer -= uiDiff;

        DoMeleeAttackIfReady();
    }
};

struct boss_rajaxxAQWarAI : public boss_rajaxxAI
{
    boss_rajaxxAQWarAI(Creature* pCreature)
        : boss_rajaxxAI(pCreature)
    {
        m_pInstance = nullptr;
        Reset();

        m_creature->SetNoXP();
        // Rajaxx is drunk with the might of C'Thun
        m_creature->SetMaxHealth(m_creature->GetMaxHealth() * 15);
        m_creature->SetHealthPercent(100);
        m_creature->SetObjectScale(7);
        m_creature->UpdateModelData();

        DoScriptText(SAY_AQ_WAR_START, m_creature);

        chargeTargetNow          = false;
        uint32 spellReflectTimer = 60000;
        uint32 AOEFearTimer      = 45000;
    }

    bool chargeTargetNow;
    uint32 spellReflectTimer;
    uint32 AOEFearTimer;

    void UpdateAI(const uint32 uiDiff)
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        // ResetAggro
        if (m_uiResetAggro_Timer < uiDiff)
        {
            m_uiResetAggro_Timer = urand(9000, 12000);
            chargeTargetNow      = true;
        }
        else
            m_uiResetAggro_Timer -= uiDiff;

        if (AOEFearTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_FEAR) == CAST_OK)
                AOEFearTimer = urand(28000, 30000);
        }
        else
            AOEFearTimer -= uiDiff;

        if (spellReflectTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_REFLECT) == CAST_OK)
            {
                spellReflectTimer = urand(28000, 30000);

                if (m_creature->GetHealthPercent() < 33.0f)
                    spellReflectTimer /= 2;
            }

            if (m_creature->GetHealthPercent() < 10)
                DoCastSpellIfCan(m_creature, SPELL_ENRAGE, CAST_AURA_NOT_PRESENT);
        }
        else
            spellReflectTimer -= uiDiff;

        /* Trash */
        if (m_uiTrash_Timer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_TRASH) == CAST_OK)
                m_uiTrash_Timer = urand(10000, 15000);
        }
        else
            m_uiTrash_Timer -= uiDiff;

        // Disarm
        if (m_uiDisarm_Timer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_DISARM, CAST_AURA_NOT_PRESENT) ==
                CAST_OK)
                m_uiDisarm_Timer = 15000;
        }
        else
            m_uiDisarm_Timer -= uiDiff;

        DoMeleeAttackIfReady();

        if (chargeTargetNow)
        {
            if (Unit* newVictim = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 1))
            {
                Unit* oldVictim = m_creature->getVictim();

                if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_CHARGE) == CAST_OK)
                {
                    m_creature->getThreatManager().modifyThreatPercent(oldVictim, -100);
                    m_creature->getThreatManager().modifyThreatPercent(newVictim, 100);

                    chargeTargetNow = false;
                }
            }
        }
    }
};

struct npc_andorovAI : public ScriptedAI
{
    npc_andorovAI(Creature* pCreature)
        : ScriptedAI(pCreature)
    {
        m_pInstance = (ScriptedInstance*)pCreature->GetInstanceData();
        Reset();
    }

    ScriptedInstance* m_pInstance;

    uint32 m_uiCommandAura_Timer;
    uint32 m_uiBash_Timer;
    uint32 m_uiStrike_Timer;

    void Reset()
    {
        m_uiCommandAura_Timer = 10000;
        m_uiBash_Timer        = 5000;
        m_uiStrike_Timer      = 15000;
        m_creature->SetWalk(false);

        if (m_pInstance && m_pInstance->GetData(TYPE_RAJAXX) == NOT_STARTED)
            m_pInstance->SetData(TYPE_GENERAL_ANDOROV, NOT_STARTED);
    }

    void JustDied(Unit* pKiller)
    {
        if (!m_pInstance)
            return;

        if (Creature* pRajaxx = m_pInstance->GetCreature(m_pInstance->GetData64(DATA_RAJAXX)))
        {
            if (pRajaxx->isAlive())
                DoScriptText(SAY_KILLS_ANDOROV, pRajaxx);
        }

        m_pInstance->SetData(TYPE_GENERAL_ANDOROV, FAIL);
    }

    void MovementInform(uint32 uiType, uint32 uiPointId)
    {
        DEBUG_EMOTE_YELL(m_creature, "DEBUG : Move inform");
        if (uiType != WAYPOINT_MOTION_TYPE)
            return;

        if (uiPointId == 1)
            DoScriptText(SAY_ANDOROV_INTRO2, m_creature);

        m_creature->SetWalk(false);
        if ((uiPointId + 1) == ANDOROV_WAYPOINT_MAX)
        {
            DEBUG_EMOTE_YELL(m_creature, "DEBUG : Andorov Done");
            m_pInstance->SetData(TYPE_GENERAL_ANDOROV, DONE);
            StartBattle();
        }
    }

    void StartEvent()
    {
        if (!m_pInstance)
            return;

        if (m_pInstance->GetData(TYPE_GENERAL_ANDOROV) == NOT_STARTED)
        {
            DoScriptText(SAY_ANDOROV_INTRO, m_creature);

            m_creature->GetMotionMaster()->MoveWaypoint(false);

            m_pInstance->SetData(TYPE_GENERAL_ANDOROV, IN_PROGRESS);
        }
        else if (m_pInstance->GetData(TYPE_GENERAL_ANDOROV) == DONE)
            StartBattle();
    }

    void StartBattle()
    {
        DEBUG_EMOTE_YELL(m_creature, "DEBUG : Start Battle");
        if (!m_pInstance)
            return;

        if (m_pInstance->GetData(TYPE_RAJAXX) == NOT_STARTED)
        {
            DoScriptText(SAY_ANDOROV_READY, m_creature);
            DoScriptText(SAY_ANDOROV_ATTACK, m_creature);
            m_creature->HandleEmoteCommand(EMOTE_STATE_READY1H);

            m_pInstance->SetData(TYPE_RAJAXX, IN_PROGRESS);
        }
    }

    void UpdateAI(const uint32 uiDiff)
    {
        if (!m_pInstance)
            return;

        // Join battle if it started without us.
        if (m_pInstance->GetData(TYPE_RAJAXX) == IN_PROGRESS &&
            m_pInstance->GetData(TYPE_GENERAL_ANDOROV) == NOT_STARTED)
            StartEvent();

        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        // Bash
        if (m_uiBash_Timer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_BASH) == CAST_OK)
                m_uiBash_Timer = 30000;
        }
        else
            m_uiBash_Timer -= uiDiff;

        // Strike
        if (m_uiStrike_Timer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_STRIKE) == CAST_OK)
                m_uiStrike_Timer = 15000;
        }
        else
            m_uiStrike_Timer -= uiDiff;

        // Aura of command
        if (m_uiCommandAura_Timer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_AURA_OF_COMMAND) == CAST_OK)
                m_uiCommandAura_Timer = 15000;
        }
        else
            m_uiCommandAura_Timer -= uiDiff;

        DoMeleeAttackIfReady();
    }
};

bool GossipHello_npc_andorov(Player* pPlayer, Creature* pCreature)
{
    pPlayer->ADD_GOSSIP_ITEM(
        GOSSIP_ICON_CHAT, GOSSIP_START, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1);
    pPlayer->SEND_GOSSIP_MENU(14442, pCreature->GetGUID());

    return true;
}

bool GossipSelect_npc_andorov(Player* pPlayer,
                              Creature* pCreature,
                              uint32 uiSender,
                              uint32 uiAction)
{
    switch (uiAction)
    {
        case GOSSIP_ACTION_INFO_DEF + 1:
            pPlayer->CLOSE_GOSSIP_MENU();
            ((npc_andorovAI*)pCreature->AI())->StartEvent();
            break;
        case GOSSIP_ACTION_TRADE: pPlayer->SEND_VENDORLIST(pCreature->GetGUID()); break;
        default: break;
    }

    return true;
}

CreatureAI* GetAI_boss_rajaxx(Creature* pCreature)
{
    if (pCreature->GetZoneId() == ZONE_SILITHUS)
        return new boss_rajaxxAQWarAI(pCreature);

    return new boss_rajaxxAI(pCreature);
}

CreatureAI* GetAI_npc_andorov(Creature* pCreature)
{
    return new npc_andorovAI(pCreature);
}

void AddSC_boss_rajaxx()
{
    Script* newscript;

    newscript                = new Script;
    newscript->Name          = "npc_andorov";
    newscript->GetAI         = &GetAI_npc_andorov;
    newscript->pGossipHello  = &GossipHello_npc_andorov;
    newscript->pGossipSelect = &GossipSelect_npc_andorov;
    newscript->RegisterSelf();

    newscript        = new Script;
    newscript->Name  = "boss_rajaxx";
    newscript->GetAI = &GetAI_boss_rajaxx;
    newscript->RegisterSelf();
}
