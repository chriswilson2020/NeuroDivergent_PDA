#pragma once
enum class ModifierKey { ALT };
struct TestKeyboard {
    template<class F>void registerKeyCombo(ModifierKey,char,F){}
};
struct TestBoard { TestKeyboard kb; };
inline TestBoard instance;
