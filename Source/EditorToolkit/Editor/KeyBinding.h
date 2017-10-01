#pragma once

#include <Urho3D/Container/Vector.h>
#include <initializer_list>

namespace Urho3D
{

class AbstractEditorInput;

/// Modifier state.
enum class ModifierState
{
    Forbidden,
    Optional,
    Required
};

/// Key binding.
class KeyBinding
{
public:
    static const KeyBinding SHIFT;
    static const KeyBinding ALT;
    static const KeyBinding CTRL;
    static const KeyBinding OPTIONAL_SHIFT;
    static const KeyBinding OPTIONAL_ALT;
    static const KeyBinding OPTIONAL_CTRL;
    static const KeyBinding ANY_MODIFIER;
    static const KeyBinding Key(int key);
    static const KeyBinding Mouse(int mouseButton);
    KeyBinding() {}
    KeyBinding(int mouseButton, int key, ModifierState shift, ModifierState alt, ModifierState ctrl);
    friend KeyBinding operator +(KeyBinding lhs, const KeyBinding& rhs);
    bool IsDown(AbstractEditorInput& input, bool ignoreGrabbed = true, bool grab = true) const;
    bool IsPressed(AbstractEditorInput& input, bool ignoreGrabbed = true, bool grab = true) const;
private:
    int mouseButton_ = -1;
    int key_ = -1;
    ModifierState shift_ = ModifierState::Forbidden;
    ModifierState alt_ = ModifierState::Forbidden;
    ModifierState ctrl_ = ModifierState::Forbidden;
};

/// Composite key binding.
class CompositeKeyBinding
{
public:
    CompositeKeyBinding() {}
    CompositeKeyBinding(const KeyBinding& keyBinding) : keyBindings_{ keyBinding } {}
    CompositeKeyBinding(const std::initializer_list<KeyBinding>& keyBindings) : keyBindings_(keyBindings) {}
    bool IsDown(AbstractEditorInput& input, bool ignoreGrabbed = true, bool grab = true) const;
    bool IsPressed(AbstractEditorInput& input, bool ignoreGrabbed = true, bool grab = true) const;
private:
    Vector<KeyBinding> keyBindings_;
};

}
