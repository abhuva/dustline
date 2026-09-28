#include "save_store.h"

#include <cstddef>
#include "bn_common.h"
#include "bn_sram.h"

namespace saves {
namespace {
BN_DATA_EWRAM_BSS slot_record slots[slot_count];
constexpr int commit_offset=int(offsetof(slot_record,header)+offsetof(slot_header,committed));
}

store::store() { refresh(); }

void store::refresh() {
    for(int index=0;index<slot_count;++index)bn::sram::read_offset(slots[index],index*slot_size);
    _active_slot=select_slot(slots[0],slots[1]);
    _generation=_active_slot>=0?slots[_active_slot].header.generation:0;
}

bool store::save(const game_state& value) {
    const int target=_active_slot==0?1:0;
    uint32_t next=_generation+1;if(!next)next=1;
    slot_record& output=slots[target];
    if(!make_slot(value,next,output))return false;
    const uint32_t commit=output.header.committed;
    output.header.committed=0;
    bn::sram::write_offset(output,target*slot_size);
    bn::sram::write_offset(commit,target*slot_size+commit_offset);
    output.header.committed=commit;
    refresh();
    return _active_slot==target && _generation==next;
}

bool store::load(game_state& value) {
    refresh();
    if(_active_slot<0)return false;
    return decode(slots[_active_slot].header.version,slots[_active_slot].payload,
                  slots[_active_slot].header.payload_size,value);
}

void store::erase() {
    const uint32_t invalid=0;
    for(int index=0;index<slot_count;++index)
        bn::sram::write_offset(invalid,index*slot_size+commit_offset);
    bn::sram::clear(slot_count*slot_size);
    refresh();
}
}
