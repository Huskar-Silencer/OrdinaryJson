#ifndef ORDINARYJSON_ORDINARY_JSON_HPP_
#define ORDINARYJSON_ORDINARY_JSON_HPP_

#include <cstdint>
#include <iosfwd>
#include <map>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace ordinaryjson {

// Base class for every exception thrown by this library. Catch this type to
// handle any error uniformly.
class OrdinaryJsonException : public std::runtime_error {
 public:
  explicit OrdinaryJsonException(const std::string& message)
      : std::runtime_error(message) {}
};

// Thrown when the input is not valid JSON. `byte()` reports the 0-based
// offset in the input where the error was detected.
class ParseError : public OrdinaryJsonException {
 public:
  ParseError(const std::string& message, size_t byte)
      : OrdinaryJsonException(message), byte_(byte) {}

  size_t byte() const noexcept { return byte_; }

 private:
  size_t byte_;
};

// Thrown when a value is accessed with an incompatible type.
class TypeError : public OrdinaryJsonException {
 public:
  explicit TypeError(const std::string& message)
      : OrdinaryJsonException(message) {}
};

// Thrown when a requested object key or array index does not exist.
class OutOfRangeError : public OrdinaryJsonException {
 public:
  explicit OutOfRangeError(const std::string& message)
      : OrdinaryJsonException(message) {}
};

class OrdinaryJsonNode {
 public:
  using JsonObjectType = std::map<std::string, OrdinaryJsonNode>;
  using JsonArrayType = std::vector<OrdinaryJsonNode>;
  using JsonStringType = std::string;

 private:
  enum class NodeValueTypeEnum {
    kObject,
    kArray,
    kString,
    kInteger,
    kDouble,
    kBool,
    kNull,
    kUnknown
  };

 public:
  OrdinaryJsonNode() : value_type_(NodeValueTypeEnum::kUnknown) {}

  explicit OrdinaryJsonNode(const JsonObjectType& value)
      : value_type_(NodeValueTypeEnum::kObject) {
    object_data_ = new JsonObjectType(value);
  }

  explicit OrdinaryJsonNode(JsonObjectType&& value)
      : value_type_(NodeValueTypeEnum::kObject) {
    object_data_ = new JsonObjectType(std::move(value));
  }

  explicit OrdinaryJsonNode(const JsonArrayType& value)
      : value_type_(NodeValueTypeEnum::kArray) {
    array_data_ = new JsonArrayType(value);
  }

  explicit OrdinaryJsonNode(JsonArrayType&& value)
      : value_type_(NodeValueTypeEnum::kArray) {
    array_data_ = new JsonArrayType(std::move(value));
  }

  explicit OrdinaryJsonNode(const JsonStringType& value)
      : value_type_(NodeValueTypeEnum::kString) {
    string_data_ = new JsonStringType(value);
  }

  explicit OrdinaryJsonNode(JsonStringType&& value)
      : value_type_(NodeValueTypeEnum::kString) {
    string_data_ = new JsonStringType(std::move(value));
  }

  template <typename T,
    std::enable_if_t<std::is_integral<T>::value &&
                   !std::is_same<T, bool>::value,
                                    int> = 0>
  explicit OrdinaryJsonNode(T value)
      : integer_data_(static_cast<int64_t>(value)),
        value_type_(NodeValueTypeEnum::kInteger) {}

  template <typename T,
    std::enable_if_t<std::is_floating_point<T>::value,
                                    int> = 0>
  explicit OrdinaryJsonNode(T value)
      : double_data_(static_cast<double>(value)),
        value_type_(NodeValueTypeEnum::kDouble) {}

  explicit OrdinaryJsonNode(const bool value)
      : boolean_data_(value), value_type_(NodeValueTypeEnum::kBool) {}

  explicit OrdinaryJsonNode(void* value)
      : null_data_(value), value_type_(NodeValueTypeEnum::kNull) {}

  OrdinaryJsonNode(const OrdinaryJsonNode& other) {
    value_type_ = other.value_type_;
    switch (value_type_) {
      case NodeValueTypeEnum::kObject:
        object_data_ = new JsonObjectType(*other.object_data_);
        break;
      case NodeValueTypeEnum::kArray:
        array_data_ = new JsonArrayType(*other.array_data_);
        break;
      case NodeValueTypeEnum::kString:
        string_data_ = new JsonStringType(*other.string_data_);
        break;
      case NodeValueTypeEnum::kInteger:
        integer_data_ = other.integer_data_;
        break;
      case NodeValueTypeEnum::kDouble:
        double_data_ = other.double_data_;
        break;
      case NodeValueTypeEnum::kBool:
        boolean_data_ = other.boolean_data_;
        break;
      case NodeValueTypeEnum::kNull:
        null_data_ = other.null_data_;
        break;
      case NodeValueTypeEnum::kUnknown:
        break;
    }
  }

  OrdinaryJsonNode(OrdinaryJsonNode&& other)  noexcept {
    value_type_ = other.value_type_;
    switch (value_type_) {
      case NodeValueTypeEnum::kObject:
        object_data_ = other.object_data_;
        other.object_data_ = nullptr;
        break;
      case NodeValueTypeEnum::kArray:
        array_data_ = other.array_data_;
        other.array_data_ = nullptr;
        break;
      case NodeValueTypeEnum::kString:
        string_data_ = other.string_data_;
        other.string_data_ = nullptr;
        break;
      case NodeValueTypeEnum::kInteger:
        integer_data_ = other.integer_data_;
        break;
      case NodeValueTypeEnum::kDouble:
        double_data_ = other.double_data_;
        break;
      case NodeValueTypeEnum::kBool:
        boolean_data_ = other.boolean_data_;
        break;
      case NodeValueTypeEnum::kNull:
        null_data_ = other.null_data_;
        break;
      case NodeValueTypeEnum::kUnknown:
        break;
    }
  }

  OrdinaryJsonNode& operator=(const OrdinaryJsonNode& other) {
    if (this == &other) return *this;
    ClearResource();
    value_type_ = other.value_type_;
    switch (value_type_) {
      case NodeValueTypeEnum::kObject:
        object_data_ = new JsonObjectType(*other.object_data_);
        break;
      case NodeValueTypeEnum::kArray:
        array_data_ = new JsonArrayType(*other.array_data_);
        break;
      case NodeValueTypeEnum::kString:
        string_data_ = new JsonStringType(*other.string_data_);
        break;
      case NodeValueTypeEnum::kInteger:
        integer_data_ = other.integer_data_;
        break;
      case NodeValueTypeEnum::kDouble:
        double_data_ = other.double_data_;
        break;
      case NodeValueTypeEnum::kBool:
        boolean_data_ = other.boolean_data_;
        break;
      case NodeValueTypeEnum::kNull:
        null_data_ = other.null_data_;
        break;
      case NodeValueTypeEnum::kUnknown:
        break;
    }
    return *this;
  }

  OrdinaryJsonNode& operator=(OrdinaryJsonNode&& other)  noexcept {
    if (this == &other) return *this;
    ClearResource();
    value_type_ = other.value_type_;
    switch (value_type_) {
      case NodeValueTypeEnum::kObject:
        object_data_ = other.object_data_;
        other.object_data_ = nullptr;
        break;
      case NodeValueTypeEnum::kArray:
        array_data_ = other.array_data_;
        other.array_data_ = nullptr;
        break;
      case NodeValueTypeEnum::kString:
        string_data_ = other.string_data_;
        other.string_data_ = nullptr;
        break;
      case NodeValueTypeEnum::kInteger:
        integer_data_ = other.integer_data_;
        break;
      case NodeValueTypeEnum::kDouble:
        double_data_ = other.double_data_;
        break;
      case NodeValueTypeEnum::kBool:
        boolean_data_ = other.boolean_data_;
        break;
      case NodeValueTypeEnum::kNull:
        null_data_ = other.null_data_;
        break;
      case NodeValueTypeEnum::kUnknown:
        break;
    }
    return *this;
  }

  ~OrdinaryJsonNode() { ClearResource(); }

  bool IsObject() const { return value_type_ == NodeValueTypeEnum::kObject; }
  bool IsArray() const { return value_type_ == NodeValueTypeEnum::kArray; }
  bool IsString() const { return value_type_ == NodeValueTypeEnum::kString; }
  bool IsInteger() const { return value_type_ == NodeValueTypeEnum::kInteger; }
  bool IsDouble() const { return value_type_ == NodeValueTypeEnum::kDouble; }
  bool IsBoolean() const { return value_type_ == NodeValueTypeEnum::kBool; }
  bool IsNull() const { return value_type_ == NodeValueTypeEnum::kNull; }
  bool HasValue() const { return value_type_ != NodeValueTypeEnum::kUnknown; }

  void Reset() {
    ClearResource();
    value_type_ = NodeValueTypeEnum::kUnknown;
  }

  void Reset(const JsonObjectType& value) {
    Reset();
    value_type_ = NodeValueTypeEnum::kObject;
    object_data_ = new JsonObjectType(value);
  }

  void Reset(JsonObjectType&& value) {
    Reset();
    value_type_ = NodeValueTypeEnum::kObject;
    object_data_ = new JsonObjectType(std::move(value));
  }

  void Reset(const JsonArrayType& value) {
    Reset();
    value_type_ = NodeValueTypeEnum::kArray;
    array_data_ = new JsonArrayType(value);
  }

  void Reset(JsonArrayType&& value) {
    Reset();
    value_type_ = NodeValueTypeEnum::kArray;
    array_data_ = new JsonArrayType(std::move(value));
  }

  void Reset(const std::string& value) {
    Reset();
    value_type_ = NodeValueTypeEnum::kString;
    string_data_ = new JsonStringType(value);
  }

  void Reset(std::string&& value) {
    Reset();
    value_type_ = NodeValueTypeEnum::kString;
    string_data_ = new JsonStringType(std::move(value));
  }

  template <typename T,
    std::enable_if_t<std::is_integral<T>::value &&
                   !std::is_same<T, bool>::value,
                                    int> = 0>
  void Reset(T value) {
    Reset();
    value_type_ = NodeValueTypeEnum::kInteger;
    integer_data_ = static_cast<int64_t>(value);
  }

  template <typename T,
    std::enable_if_t<std::is_floating_point<T>::value,
                                    int> = 0>
  void Reset(T value) {
    Reset();
    value_type_ = NodeValueTypeEnum::kDouble;
    double_data_ = static_cast<double>(value);
  }

  void Reset(const bool value) {
    Reset();
    value_type_ = NodeValueTypeEnum::kBool;
    boolean_data_ = value;
  }

  JsonObjectType& GetObjectNodeRef() {
    if (!IsObject()) RejectMismatchedType("object", GetValueTypeToString());
    return *object_data_;
  }

  const JsonObjectType& GetObjectNodeRef() const {
    if (!IsObject()) RejectMismatchedType("object", GetValueTypeToString());
    return *object_data_;
  }

  JsonArrayType& GetArrayNodeRef() {
    if (!IsArray()) RejectMismatchedType("array", GetValueTypeToString());
    return *array_data_;
  }

  const JsonArrayType& GetArrayNodeRef() const {
    if (!IsArray()) RejectMismatchedType("array", GetValueTypeToString());
    return *array_data_;
  }

  OrdinaryJsonNode& GetAsObject(const std::string& key) {
    if (!IsObject()) RejectMismatchedType("object", GetValueTypeToString());
    const auto it = object_data_->find(key);
    if (it == object_data_->end())
      throw OutOfRangeError("key '" + key + "' not found");
    return it->second;
  }

  const OrdinaryJsonNode& GetAsObject(const std::string& key) const {
    if (!IsObject()) RejectMismatchedType("object", GetValueTypeToString());
    const auto it = object_data_->find(key);
    if (it == object_data_->end())
      throw OutOfRangeError("key '" + key + "' not found");
    return it->second;
  }

  OrdinaryJsonNode& GetAsArray(const size_t index) {
    if (!IsArray()) RejectMismatchedType("array", GetValueTypeToString());
    if (index >= array_data_->size())
      throw OutOfRangeError("index " + std::to_string(index) +
                            " is out of range");
    return array_data_->at(index);
  }

  const OrdinaryJsonNode& GetAsArray(const size_t index) const {
    if (!IsArray()) RejectMismatchedType("array", GetValueTypeToString());
    if (index >= array_data_->size())
      throw OutOfRangeError("index " + std::to_string(index) +
                            " is out of range");
    return array_data_->at(index);
  }

  JsonStringType& GetAsString() {
    if (!IsString()) RejectMismatchedType("string", GetValueTypeToString());
    return *string_data_;
  }

  const JsonStringType& GetAsString() const {
    if (!IsString()) RejectMismatchedType("string", GetValueTypeToString());
    return *string_data_;
  }

  int64_t& GetAsInteger() {
    if (!IsInteger()) RejectMismatchedType("integer", GetValueTypeToString());
    return integer_data_;
  }

  const int64_t& GetAsInteger() const {
    if (!IsInteger()) RejectMismatchedType("integer", GetValueTypeToString());
    return integer_data_;
  }

  double& GetAsDouble() {
    if (!IsDouble()) RejectMismatchedType("double", GetValueTypeToString());
    return double_data_;
  }

  const double& GetAsDouble() const {
    if (!IsDouble()) RejectMismatchedType("double", GetValueTypeToString());
    return double_data_;
  }

  bool& GetAsBool() {
    if (!IsBoolean()) RejectMismatchedType("boolean", GetValueTypeToString());
    return boolean_data_;
  }

  const bool& GetAsBool() const {
    if (!IsBoolean()) RejectMismatchedType("boolean", GetValueTypeToString());
    return boolean_data_;
  }

  NodeValueTypeEnum GetValueType() const { return value_type_; }

  std::string GetValueTypeToString() const;

  std::string Stringify() const;

 private:
  union {
    JsonObjectType* object_data_{};
    JsonArrayType* array_data_;
    JsonStringType* string_data_;
    int64_t integer_data_;
    double double_data_;
    bool boolean_data_;
    void* null_data_;
  };

  NodeValueTypeEnum value_type_;

  void ClearResource() {
    switch (value_type_) {
      case NodeValueTypeEnum::kObject:
        delete object_data_;
        object_data_ = nullptr;
        break;
      case NodeValueTypeEnum::kArray:
        delete array_data_;
        array_data_ = nullptr;
        break;
      case NodeValueTypeEnum::kString:
        delete string_data_;
        string_data_ = nullptr;
        break;
      case NodeValueTypeEnum::kInteger:
        integer_data_ = 0;
        break;
      case NodeValueTypeEnum::kDouble:
        double_data_ = 0;
        break;
      case NodeValueTypeEnum::kBool:
        boolean_data_ = false;
        break;
      case NodeValueTypeEnum::kNull:
        null_data_ = nullptr;
        break;
      case NodeValueTypeEnum::kUnknown:
        break;
    }
  }

  static void RejectMismatchedType(const std::string& expected_type,
                                   const std::string& actual_type);

  void StringifyTo(std::ostream& os) const;
};

namespace internal {

using JsonObjectType = std::map<std::string, OrdinaryJsonNode>;
using JsonArrayType = std::vector<OrdinaryJsonNode>;
using JsonStringType = std::string;

}  // namespace internal

OrdinaryJsonNode Parse(const std::string& value);
std::string Stringify(const OrdinaryJsonNode& value);

}  // namespace ordinaryjson

#endif  // ORDINARYJSON_ORDINARY_JSON_HPP_
