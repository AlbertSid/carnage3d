#include "stdafx.h"
#include "config_document.h"
#include "cJSON.h"

namespace cxx
{

config_node::config_node(cJSON* jsonElementImplementation)
    : mJsonElement(jsonElementImplementation)
{
}

config_node::~config_node()
{
}

config_node config_node::get_child(const char* name) const
{
    if (mJsonElement == nullptr)
        return config_node { nullptr };

    cJSON* element = cJSON_GetObjectItem(mJsonElement, name);
    return config_node { element };
}

config_node config_node::next_sibling() const
{
    if (mJsonElement == nullptr)
        return config_node { nullptr };

    cJSON* element = mJsonElement->next;
    return config_node { element };
}

config_node config_node::prev_sibling() const
{
    if (mJsonElement == nullptr)
        return config_node { nullptr };

    cJSON* element = mJsonElement->prev;
    return config_node { element };
}

config_node config_node::first_child() const
{
    if (mJsonElement == nullptr)
        return config_node { nullptr };

    cJSON* element = mJsonElement->child;
    return config_node { element };
}

config_node config_node::get_array_element(int elementIndex) const
{
    if (mJsonElement == nullptr)
        return config_node { nullptr };

    cJSON* element = cJSON_GetArrayItem(mJsonElement, elementIndex);
    return config_node { element };
}

int config_node::get_array_elements_count() const
{
    if (mJsonElement == nullptr)
        return 0;

    return cJSON_GetArraySize(mJsonElement);
}

bool config_node::is_child_exists(const char* name) const
{
    if (mJsonElement == nullptr)
        return false;

    return cJSON_HasObjectItem(mJsonElement, name) > 0;
}

const char* config_node::get_element_name() const
{
    if (mJsonElement == nullptr || mJsonElement->string == nullptr)
        return "";

    return mJsonElement->string;
}

bool config_node::is_string_element() const
{
    return mJsonElement && mJsonElement->type == cJSON_String;
}

bool config_node::is_number_element() const
{
    return mJsonElement && mJsonElement->type == cJSON_Number;
}

bool config_node::is_boolean_element() const
{
    return mJsonElement && (mJsonElement->type == cJSON_True || mJsonElement->type == cJSON_False);
}

bool config_node::is_array_element() const
{
    return mJsonElement && mJsonElement->type == cJSON_Array;
}

bool config_node::is_object_element() const
{
    return mJsonElement && mJsonElement->type == cJSON_Object;
}

const char* config_node::get_value_string() const
{
    debug_assert(is_string_element());

    if (mJsonElement == nullptr || mJsonElement->valuestring == nullptr)
        return "";

    return mJsonElement->valuestring;
}

bool config_node::get_value_boolean() const
{
    debug_assert(is_boolean_element());
    return mJsonElement && mJsonElement->valueint != 0;
}

int config_node::get_value_integer() const
{
    debug_assert(is_number_element());
    if (mJsonElement == nullptr)
        return 0;

    return mJsonElement->valueint;
}

float config_node::get_value_float() const
{
    debug_assert(is_number_element());
    if (mJsonElement == nullptr)
        return 0.0f;

    return static_cast<float>(mJsonElement->valuedouble);
}

//////////////////////////////////////////////////////////////////////////

config_document::config_document(const char* content)
    : mJsonElement()
{
    parse_document(content);
}

config_document::config_document()
    : mJsonElement()
{
}

config_document::~config_document()
{
    close_document();
}

bool config_document::parse_document(const char* content)
{
    close_document();

    mJsonElement = cJSON_Parse(content);
    return mJsonElement != nullptr;
}

void config_document::close_document()
{
    if (mJsonElement)
    {
        cJSON_Delete(mJsonElement);
        mJsonElement = nullptr;
    }
}

config_node config_document::get_root_node() const
{
    return config_node { mJsonElement };
}

bool config_document::is_loaded() const
{
    return mJsonElement != nullptr;
}

} // namespace cxx