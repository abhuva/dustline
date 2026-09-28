#pragma once

#include "save_data.h"

namespace saves {

enum class result {
    none=0,saved=1,loaded=2,erased=3,no_save=4,invalid=5,incompatible=6,
    loaded_updated=7,loaded_relocated=8
};

class store {
public:
    store();
    void refresh();
    bool has_save() const { return _active_slot>=0; }
    uint32_t generation() const { return _generation; }
    bool save(const game_state& value);
    bool load(game_state& value);
    void erase();
private:
    int _active_slot=-1;
    uint32_t _generation=0;
};
}
