#pragma once
#include "bn_optional.h"
#include "bn_regular_bg_ptr.h"
#include "bn_sprite_ptr.h"
#include "bn_vector.h"
#include "garage_shop.h"

class town_scene {
public:
    struct rect { int left,top,right,bottom; };
    enum class place { exterior, garage };
    enum class event { none, redraw, menu_opened, menu_closed, setup_applied, shop_purchase_requested,
                       weapon_fitting_opened, contract_opened, race_opened, return_to_world };

    town_scene(int town_id,int setup,const garage_shop::ownership& owned);
    event update(int& setup);
    void set_visible(bool visible);
    void suspend();
    void resume();
    void return_to_race_building();

    int town_id() const { return _town_id; }
    int x() const { return _x; }
    int y() const { return _y; }
    int direction() const { return _direction; }
    int menu_selection() const { return _menu_selection; }
    int menu_page() const { return _menu_page; }
    int shop_selection() const { return _shop_selection; }
    int shop_count() const { return _shop_count; }
    int shop_item() const;
    void refresh_shop(const garage_shop::ownership& owned);
    bool shop_info_open() const { return _shop_info_open; }
    place current_place() const { return _place; }
    bool menu_open() const { return _menu_open; }
    bool prompt_visible() const { return _prompt.visible(); }
    bool player_visible() const { return _player.visible(); }

private:
    void _load(place next);
    void _refresh_sprite();
    bool _blocked(int x,int y) const;
    bool _inside(const rect& area,int x,int y) const;

    bn::optional<bn::regular_bg_ptr> _background;
    bn::sprite_ptr _player;
    bn::sprite_ptr _prompt;
    bn::sprite_ptr _shop_cursor;
    bn::vector<bn::sprite_ptr,garage_shop::max_visible_stock> _shop_icons;
    uint8_t _visible_stock[garage_shop::max_visible_stock]{};
    int _town_id;
    int _x=128,_y=226;
    int _direction=3;
    int _walk_ticks=0;
    int _prompt_ticks=0;
    int _menu_selection=0;
    int _menu_page=0,_shop_selection=0;
    int _shop_count=0;
    place _place=place::exterior;
    bool _menu_open=false;
    bool _shop_info_open=false;
    bool _visible=true;
};
