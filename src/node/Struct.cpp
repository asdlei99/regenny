#include <imgui.h>

#include "Array.hpp"
#include "Pointer.hpp"
#include "Undefined.hpp"

#include "Struct.hpp"

namespace node {
Struct::Struct(Process& process, genny::Variable* var)
    : Variable{process, var}, m_struct{dynamic_cast<genny::Struct*>(var->type())}, m_size{var->size()} {
    assert(m_struct != nullptr);

    // Build the node map.
    auto make_node = [&](genny::Variable* var) -> std::unique_ptr<Base> {
        if (var->type()->is_a<genny::Array>()) {
            return std::make_unique<Array>(m_process, var);
        } else if (var->type()->is_a<genny::Struct>()) {
            return std::make_unique<Struct>(m_process, var);
        } else if (var->type()->is_a<genny::Pointer>()) {
            return std::make_unique<Pointer>(m_process, var);
        } else {
            return std::make_unique<Variable>(m_process, var);
        }
    };
    std::function<void(genny::Struct*)> add_vars = [&](genny::Struct* s) {
        for (auto&& parent : s->parents()) {
            add_vars(parent);
        }

        for (auto&& var : s->get_all<genny::Variable>()) {
            m_nodes[var->offset()] = make_node(var);
        }
    };

    add_vars(m_struct);

    // Fill in the rest of the offsets with undefined nodes.
    uintptr_t last_offset{};

    auto fill_space = [&](int delta) {
        switch (delta) {
        case 8:
            m_nodes[last_offset] = std::make_unique<Undefined>(m_process, 8);
            break;

        case 7:
            m_nodes[last_offset] = std::make_unique<Undefined>(m_process, 4);
            m_nodes[last_offset + 4] = std::make_unique<Undefined>(m_process, 2);
            m_nodes[last_offset + 6] = std::make_unique<Undefined>(m_process, 1);
            break;

        case 6:
            m_nodes[last_offset] = std::make_unique<Undefined>(m_process, 4);
            m_nodes[last_offset + 4] = std::make_unique<Undefined>(m_process, 2);
            break;

        case 5:
            m_nodes[last_offset] = std::make_unique<Undefined>(m_process, 4);
            m_nodes[last_offset + 4] = std::make_unique<Undefined>(m_process, 1);
            break;

        case 4:
            m_nodes[last_offset] = std::make_unique<Undefined>(m_process, 4);
            break;

        case 3:
            m_nodes[last_offset] = std::make_unique<Undefined>(m_process, 2);
            m_nodes[last_offset + 2] = std::make_unique<Undefined>(m_process, 1);
            break;

        case 2:
            m_nodes[last_offset] = std::make_unique<Undefined>(m_process, 2);
            break;

        case 1:
            m_nodes[last_offset] = std::make_unique<Undefined>(m_process, 1);
            break;

        default:
            break;
        }
    };

    for (uintptr_t offset = 0; offset < m_struct->size();) {
        auto delta = offset - last_offset;

        if (auto search = m_nodes.find(offset); search != m_nodes.end()) {
            auto& node = search->second;
            fill_space(delta);
            last_offset = offset + node->size();
            offset = last_offset;
        } else if (delta > 0 && offset % sizeof(uintptr_t) == 0) {
            fill_space(delta);
            last_offset = offset;
            ++offset;
        } else {
            ++offset;
        }
    }

    fill_space(m_struct->size() - last_offset);
}

void Struct::display(uintptr_t address, uintptr_t offset, std::byte* mem) {
    if (m_display_self) {
        ImGui::BeginGroup();
        display_address_offset(address, offset);
        ImGui::SameLine();
        display_type();
        ImGui::SameLine();
        display_name();
        ImGui::EndGroup();

        if (ImGui::BeginPopupContextItem("ArrayNode")) {
            ImGui::Checkbox("Collpase", &m_is_collapsed);
            ImGui::EndPopup();
        }

        if (m_is_collapsed) {
            return;
        }
    }

    for (uintptr_t offset = 0; offset < m_struct->size();) {
        if (auto search = m_nodes.find(offset); search != m_nodes.end()) {
            auto& node = search->second;

            if (m_display_self) {
                ++indentation_level;
            }

            ImGui::PushID(node.get());
            node->display(address + offset, offset, &mem[offset]);
            ImGui::PopID();

            if (m_display_self) {
                --indentation_level;
            }

            offset += node->size();
        } else {
            ++offset;
        }
    }
}

size_t Struct::size() {
    return m_size;
}
} // namespace node
