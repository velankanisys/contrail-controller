/*
 * Copyright (c) 2013 Juniper Networks, Inc. All rights reserved.
 */

#include <uve/agent_uve.h>
#include <uve/vn_uve_table.h>
#include <uve/vn_uve_entry.h>

VnUveTable::VnUveTable(Agent *agent)
    : VnUveTableBase(agent) {
}

VnUveTable::~VnUveTable() {
}

VnUveTableBase::VnUveEntryPtr VnUveTable::Allocate(const VnEntry *vn) {
    VnUveEntryPtr uve(new VnUveEntry(agent_, vn));
    return uve;
}

VnUveTableBase::VnUveEntryPtr VnUveTable::Allocate() {
    VnUveEntryPtr uve(new VnUveEntry(agent_));
    return uve;
}

bool VnUveTable::SendUnresolvedVnMsg(const string &vn_name,
                                     UveVirtualNetworkAgent &uve) {
    UveVnMap::iterator it = uve_vn_map_.find(vn_name);
    if (it == uve_vn_map_.end()) {
        return false;
    }
    VnUveEntryPtr entry_ptr(it->second);
    VnUveEntry *entry = static_cast<VnUveEntry *>(entry_ptr.get());

    bool changed = false;
    uve.set_name(vn_name);
    changed = entry->PopulateInterVnStats(uve);


    AgentUve *u = static_cast<AgentUve *>(agent_->uve());
    StatsManager *stats = u->stats_manager();
    /* Send Nameless VrfStats as part of Unknown VN */
    if (vn_name.compare(FlowHandler::UnknownVn()) == 0) {
        changed = entry->FillVrfStats(stats->GetNamelessVrfId(), uve);
    }

    return changed;
}

void VnUveTable::SendVnStats(bool only_vrf_stats) {
    UveVnMap::const_iterator it = uve_vn_map_.begin();
    while (it != uve_vn_map_.end()) {
        const VnUveEntry *entry = static_cast<VnUveEntry *>(it->second.get());
        if (entry->vn()) {
            SendVnStatsMsg(entry->vn(), only_vrf_stats);
        }
        ++it;
    }
    UveVirtualNetworkAgent uve1, uve2;
    if (SendUnresolvedVnMsg(FlowHandler::UnknownVn(), uve1)) {
        DispatchVnMsg(uve1);
    }
    if (SendUnresolvedVnMsg(FlowHandler::LinkLocalVn(), uve1)) {
        DispatchVnMsg(uve2);
    }
}

void VnUveTable::SendVnStatsMsg(const VnEntry *vn, bool only_vrf_stats) {
    VnUveEntry* entry = static_cast<VnUveEntry*>(UveEntryFromVn(vn));
    if (entry == NULL) {
        return;
    }
    UveVirtualNetworkAgent uve;

    bool send = entry->FrameVnStatsMsg(vn, uve, only_vrf_stats);
    if (send) {
        DispatchVnMsg(uve);
    }
}

void VnUveTable::UpdateBitmap(const string &vn, uint8_t proto, uint16_t sport,
                              uint16_t dport) {
    UveVnMap::iterator it = uve_vn_map_.find(vn);
    if (it == uve_vn_map_.end()) {
        return;
    }

    VnUveEntry * entry = static_cast<VnUveEntry *>(it->second.get());
    entry->UpdatePortBitmap(proto, sport, dport);
}

void VnUveTable::UpdateInterVnStats(const string &src, const string &dst,
                                    uint64_t bytes, uint64_t pkts,
                                    bool outgoing) {
    UveVnMap::iterator it = uve_vn_map_.find(src);
    if (it == uve_vn_map_.end()) {
        return;
    }

    VnUveEntry * entry = static_cast<VnUveEntry *>(it->second.get());
    entry->UpdateInterVnStats(dst, bytes, pkts, outgoing);
}

void VnUveTable::RemoveInterVnStats(const string &vn) {
    UveVnMap::iterator it = uve_vn_map_.find(vn);
    if (it == uve_vn_map_.end()) {
        return;
    }

    VnUveEntry * entry = static_cast<VnUveEntry *>(it->second.get());
    entry->ClearInterVnStats();
}

void VnUveTable::Delete(const VnEntry *vn) {
    RemoveInterVnStats(vn->GetName());
    VnUveTableBase::Delete(vn);
}

