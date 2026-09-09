#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace kite {

enum class ValueType : std::uint8_t { Nil, Bool, Int, Float, String, Array, Map };

class Value;

struct Object {
    std::uint32_t refcount = 1;
    ValueType type;
    explicit Object(ValueType t) : type(t) {}
    virtual ~Object() = default;
};

struct StringObject final : Object {
    std::string data;
    explicit StringObject(std::string value) : Object(ValueType::String), data(std::move(value)) {}
};

struct ArrayObject final : Object {
    std::vector<Value> elements;
    ArrayObject() : Object(ValueType::Array) {}
};

struct MapObject final : Object {
    std::unordered_map<std::string, Value> entries;
    std::string type_name;
    MapObject() : Object(ValueType::Map) {}
};

class Value {
public:
    Value() noexcept : type_(ValueType::Nil), int_(0) {}
    Value(bool value) noexcept : type_(ValueType::Bool), int_(value ? 1 : 0) {}
    Value(std::int64_t value) noexcept : type_(ValueType::Int), int_(value) {}
    Value(int value) noexcept : type_(ValueType::Int), int_(value) {}
    Value(double value) noexcept : type_(ValueType::Float), float_(value) {}

    static Value string(std::string value) { return Value(new StringObject(std::move(value))); }
    static Value array() { return Value(new ArrayObject()); }
    static Value map() { return Value(new MapObject()); }

    Value(const Value& other) noexcept : type_(other.type_), int_(other.int_) { retain(); }
    Value(Value&& other) noexcept : type_(other.type_), int_(other.int_) {
        other.type_ = ValueType::Nil;
        other.int_ = 0;
    }
    Value& operator=(const Value& other) noexcept {
        if (this != &other) {
            release();
            type_ = other.type_;
            int_ = other.int_;
            retain();
        }
        return *this;
    }
    Value& operator=(Value&& other) noexcept {
        if (this != &other) {
            release();
            type_ = other.type_;
            int_ = other.int_;
            other.type_ = ValueType::Nil;
            other.int_ = 0;
        }
        return *this;
    }
    ~Value() { release(); }

    ValueType type() const noexcept { return type_; }
    bool is_nil() const noexcept { return type_ == ValueType::Nil; }
    bool is_bool() const noexcept { return type_ == ValueType::Bool; }
    bool is_int() const noexcept { return type_ == ValueType::Int; }
    bool is_float() const noexcept { return type_ == ValueType::Float; }
    bool is_number() const noexcept { return type_ == ValueType::Int || type_ == ValueType::Float; }
    bool is_string() const noexcept { return type_ == ValueType::String; }
    bool is_array() const noexcept { return type_ == ValueType::Array; }
    bool is_map() const noexcept { return type_ == ValueType::Map; }
    bool is_truthy() const noexcept { return type_ == ValueType::Bool && int_ != 0; }

    bool as_bool() const noexcept { return int_ != 0; }
    std::int64_t as_int() const noexcept { return int_; }
    double as_float() const noexcept { return float_; }
    double as_number() const noexcept {
        return type_ == ValueType::Int ? static_cast<double>(int_) : float_;
    }
    std::string& as_string() const noexcept { return object<StringObject>()->data; }
    std::vector<Value>& as_array() const noexcept { return object<ArrayObject>()->elements; }
    std::unordered_map<std::string, Value>& as_map() const noexcept { return object<MapObject>()->entries; }
    std::string& map_type() const noexcept { return object<MapObject>()->type_name; }

    const void* identity() const noexcept { return obj_; }
    bool equals(const Value& other) const;

private:
    explicit Value(Object* pointer) noexcept : type_(pointer->type), obj_(pointer) {}

    bool is_object() const noexcept { return type_ >= ValueType::String; }
    void retain() noexcept { if (is_object()) ++obj_->refcount; }
    void release() noexcept {
        if (is_object() && --obj_->refcount == 0) delete obj_;
    }
    template <typename T>
    T* object() const noexcept { return static_cast<T*>(obj_); }

    ValueType type_;
    union {
        std::int64_t int_;
        double float_;
        Object* obj_;
    };
};

std::string to_string(const Value& value);
const char* type_name(const Value& value);

} // namespace kite
