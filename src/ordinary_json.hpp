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
  explicit OrdinaryJsonException(const std::string &message)
      : std::runtime_error(message) {}
};

// Thrown when the input is not valid JSON. `byte()` reports the 0-based
// offset in the input where the error was detected.
class ParseError : public OrdinaryJsonException {
 public:
  ParseError(const std::string &message, size_t byte)
      : OrdinaryJsonException(message), byte_(byte) {}

  size_t byte() const noexcept { return byte_; }

 private:
  size_t byte_;
};

// Thrown when a value is accessed with an incompatible type.
class TypeError : public OrdinaryJsonException {
 public:
  explicit TypeError(const std::string &message)
      : OrdinaryJsonException(message) {}
};

// Thrown when a requested object key or array index does not exist.
class OutOfRangeError : public OrdinaryJsonException {
 public:
  explicit OutOfRangeError(const std::string &message)
      : OrdinaryJsonException(message) {}
};

/**
 * @brief A JSON value node.
 *
 * A node holds exactly one value at a time:
 *
 * | Kind    | Backing type                        |
 * |---------|-------------------------------------|
 * | object  | `JsonObjectType` (std::map)         |
 * | array   | `JsonArrayType` (std::vector)       |
 * | string  | `JsonStringType` (std::string)      |
 * | integer | `int64_t`                           |
 * | double  | `double`                            |
 * | bool    | `bool`                              |
 * | null    | `void*` (always null)               |
 * | unknown | a valueless, default-constructed node |
 *
 * The active value lives in a union, so the node is not thread-safe. Build a
 * tree with `Parse()`, inspect/mutate it through the `Is*` / `GetAs*` APIs,
 * and serialize it back with `Stringify()`.
 */
class OrdinaryJsonNode {
 public:
  /// Alias for the underlying object container.
  using JsonObjectType = std::map<std::string, OrdinaryJsonNode>;
  /// Alias for the underlying array container.
  using JsonArrayType = std::vector<OrdinaryJsonNode>;
  /// Alias for the underlying string type.
  using JsonStringType = std::string;

  /// The concrete value type held by a node. `kUnknown` means the node is
  /// valueless (default-constructed, moved-from, or `Reset()`).
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
  /// Constructs a valueless (kUnknown) node.
  OrdinaryJsonNode() : value_type_(NodeValueTypeEnum::kUnknown) {}

  /**
   * @brief Constructs an object node from a copy of `value`.
   * @param value The object to copy.
   */
  explicit OrdinaryJsonNode(const JsonObjectType &value)
      : value_type_(NodeValueTypeEnum::kObject) {
    object_data_ = new JsonObjectType(value);
  }

  /**
   * @brief Constructs an object node by moving `value`.
   * @param value The object to move.
   */
  explicit OrdinaryJsonNode(JsonObjectType &&value)
      : value_type_(NodeValueTypeEnum::kObject) {
    object_data_ = new JsonObjectType(std::move(value));
  }

  /**
   * @brief Constructs an array node from a copy of `value`.
   * @param value The array to copy.
   */
  explicit OrdinaryJsonNode(const JsonArrayType &value)
      : value_type_(NodeValueTypeEnum::kArray) {
    array_data_ = new JsonArrayType(value);
  }

  /**
   * @brief Constructs an array node by moving `value`.
   * @param value The array to move.
   */
  explicit OrdinaryJsonNode(JsonArrayType &&value)
      : value_type_(NodeValueTypeEnum::kArray) {
    array_data_ = new JsonArrayType(std::move(value));
  }

  /**
   * @brief Constructs a string node from a copy of `value`.
   * @param value The string to copy.
   */
  explicit OrdinaryJsonNode(const JsonStringType &value)
      : value_type_(NodeValueTypeEnum::kString) {
    string_data_ = new JsonStringType(value);
  }

  /**
   * @brief Constructs a string node by moving `value`.
   * @param value The string to move.
   */
  explicit OrdinaryJsonNode(JsonStringType &&value)
      : value_type_(NodeValueTypeEnum::kString) {
    string_data_ = new JsonStringType(std::move(value));
  }

  /**
   * @brief Constructs an integer node from any integral value except `bool`.
   * @tparam T An integral type (the `bool` overload below takes precedence).
   * @param value The integer value, stored as `int64_t`.
   */
  template <typename T, std::enable_if_t<std::is_integral<T>::value &&
                                             !std::is_same<T, bool>::value,
                                         int> = 0>
  explicit OrdinaryJsonNode(T value)
      : integer_data_(static_cast<int64_t>(value)),
        value_type_(NodeValueTypeEnum::kInteger) {}

  /**
   * @brief Constructs a double node from any floating-point value.
   * @tparam T A floating-point type.
   * @param value The value, stored as `double`.
   */
  template <typename T,
            std::enable_if_t<std::is_floating_point<T>::value, int> = 0>
  explicit OrdinaryJsonNode(T value)
      : double_data_(static_cast<double>(value)),
        value_type_(NodeValueTypeEnum::kDouble) {}

  /**
   * @brief Constructs a bool node.
   * @param value The boolean value.
   *
   * Note: there is no `const char*` constructor, so
   * `OrdinaryJsonNode("text")` resolves to this `bool` overload. Use
   * `std::string(...)` for string nodes, and
   * `OrdinaryJsonNode(static_cast<void*>(nullptr))` for null.
   */
  explicit OrdinaryJsonNode(const bool value)
      : boolean_data_(value), value_type_(NodeValueTypeEnum::kBool) {}

  /**
   * @brief Constructs a null node.
   * @param value Ignored; pass `static_cast<void*>(nullptr)`.
   */
  explicit OrdinaryJsonNode(void *value)
      : null_data_(value), value_type_(NodeValueTypeEnum::kNull) {}

  /**
   * @brief Constructs a deep copy of `other`.
   *
   * Heap-backed values (object/array/string) are duplicated so the copy is
   * fully independent; scalar values (integer/double/bool/null) are copied by
   * value.
   */
  OrdinaryJsonNode(const OrdinaryJsonNode &other) { copyFrom(other); }

  /**
   * @brief Moves the contents of `other` into the new node.
   *
   * `other` is left in a valid, valueless (kUnknown) state.
   */
  OrdinaryJsonNode(OrdinaryJsonNode &&other) noexcept { moveFrom(other); }

  /**
   * @brief Replaces this node with a deep copy of `other`.
   * @return `*this`. Self-assignment is safe.
   */
  OrdinaryJsonNode &operator=(const OrdinaryJsonNode &other) {
    if (this == &other) return *this;
    clearResource();
    copyFrom(other);
    return *this;
  }

  /**
   * @brief Replaces this node with the contents of `other`.
   * @return `*this`. `other` is left valueless (kUnknown). Self-assignment is
   *         safe.
   */
  OrdinaryJsonNode &operator=(OrdinaryJsonNode &&other) noexcept {
    if (this == &other) return *this;
    clearResource();
    moveFrom(other);
    return *this;
  }

  /// Releases any owned resources.
  ~OrdinaryJsonNode() { clearResource(); }

  // Type predicates ----------------------------------------------------------
  /// @return true if the node holds an object value.
  bool IsObject() const { return value_type_ == NodeValueTypeEnum::kObject; }
  /// @return true if the node holds an array value.
  bool IsArray() const { return value_type_ == NodeValueTypeEnum::kArray; }
  /// @return true if the node holds a string value.
  bool IsString() const { return value_type_ == NodeValueTypeEnum::kString; }
  /// @return true if the node holds an integer value.
  bool IsInteger() const { return value_type_ == NodeValueTypeEnum::kInteger; }
  /// @return true if the node holds a double value.
  bool IsDouble() const { return value_type_ == NodeValueTypeEnum::kDouble; }
  /// @return true if the node holds a bool value.
  bool IsBoolean() const { return value_type_ == NodeValueTypeEnum::kBool; }
  /// @return true if the node holds a null value.
  bool IsNull() const { return value_type_ == NodeValueTypeEnum::kNull; }
  /// @return false if the node is valueless (kUnknown).
  bool HasValue() const { return value_type_ != NodeValueTypeEnum::kUnknown; }

  // Reset: clear the node, then store a new value ---------------------------
  /// Clears the node, leaving it valueless (kUnknown).
  void Reset() {
    clearResource();
    value_type_ = NodeValueTypeEnum::kUnknown;
  }

  /// Clears the node, then stores a copy of `value` as an object.
  void Reset(const JsonObjectType &value) {
    Reset();
    value_type_ = NodeValueTypeEnum::kObject;
    object_data_ = new JsonObjectType(value);
  }

  /// Clears the node, then stores `value` as an object (moved).
  void Reset(JsonObjectType &&value) {
    Reset();
    value_type_ = NodeValueTypeEnum::kObject;
    object_data_ = new JsonObjectType(std::move(value));
  }

  /// Clears the node, then stores a copy of `value` as an array.
  void Reset(const JsonArrayType &value) {
    Reset();
    value_type_ = NodeValueTypeEnum::kArray;
    array_data_ = new JsonArrayType(value);
  }

  /// Clears the node, then stores `value` as an array (moved).
  void Reset(JsonArrayType &&value) {
    Reset();
    value_type_ = NodeValueTypeEnum::kArray;
    array_data_ = new JsonArrayType(std::move(value));
  }

  /// Clears the node, then stores a copy of `value` as a string.
  void Reset(const std::string &value) {
    Reset();
    value_type_ = NodeValueTypeEnum::kString;
    string_data_ = new JsonStringType(value);
  }

  /// Clears the node, then stores `value` as a string (moved).
  void Reset(std::string &&value) {
    Reset();
    value_type_ = NodeValueTypeEnum::kString;
    string_data_ = new JsonStringType(std::move(value));
  }

  /**
   * @brief Clears the node, then stores an integer value.
   * @tparam T An integral type except `bool` (the `bool` overload wins).
   * @param value The value, stored as `int64_t`.
   */
  template <typename T, std::enable_if_t<std::is_integral<T>::value &&
                                             !std::is_same<T, bool>::value,
                                         int> = 0>
  void Reset(T value) {
    Reset();
    value_type_ = NodeValueTypeEnum::kInteger;
    integer_data_ = static_cast<int64_t>(value);
  }

  /**
   * @brief Clears the node, then stores a double value.
   * @tparam T A floating-point type.
   * @param value The value, stored as `double`.
   */
  template <typename T,
            std::enable_if_t<std::is_floating_point<T>::value, int> = 0>
  void Reset(T value) {
    Reset();
    value_type_ = NodeValueTypeEnum::kDouble;
    double_data_ = static_cast<double>(value);
  }

  /// Clears the node, then stores a bool value.
  void Reset(const bool value) {
    Reset();
    value_type_ = NodeValueTypeEnum::kBool;
    boolean_data_ = value;
  }

  /**
   * @brief Clears the node, then stores a null value.
   * @param value Ignored; pass `static_cast<void*>(nullptr)`.
   */
  void Reset(void *value) {
    Reset();
    value_type_ = NodeValueTypeEnum::kNull;
    null_data_ = value;
  }

  // Container accessors ------------------------------------------------------
  /**
   * @brief Returns a mutable reference to the underlying object map.
   * @return `std::map<std::string, OrdinaryJsonNode>&`.
   * @throw TypeError if the node is not an object.
   */
  JsonObjectType &GetObjectNodeRef() {
    if (!IsObject()) rejectMismatchedType("object", GetValueTypeToString());
    return *object_data_;
  }

  /**
   * @brief Returns a const reference to the underlying object map.
   * @return `const std::map<std::string, OrdinaryJsonNode>&`.
   * @throw TypeError if the node is not an object.
   */
  const JsonObjectType &GetObjectNodeRef() const {
    if (!IsObject()) rejectMismatchedType("object", GetValueTypeToString());
    return *object_data_;
  }

  /**
   * @brief Returns a mutable reference to the underlying array vector.
   * @return `std::vector<OrdinaryJsonNode>&`.
   * @throw TypeError if the node is not an array.
   */
  JsonArrayType &GetArrayNodeRef() {
    if (!IsArray()) rejectMismatchedType("array", GetValueTypeToString());
    return *array_data_;
  }

  /**
   * @brief Returns a const reference to the underlying array vector.
   * @return `const std::vector<OrdinaryJsonNode>&`.
   * @throw TypeError if the node is not an array.
   */
  const JsonArrayType &GetArrayNodeRef() const {
    if (!IsArray()) rejectMismatchedType("array", GetValueTypeToString());
    return *array_data_;
  }

  /**
   * @brief Returns a mutable reference to the value stored under `key`.
   * @param key The object key.
   * @throw TypeError if the node is not an object.
   * @throw OutOfRangeError if `key` does not exist.
   */
  OrdinaryJsonNode &GetAsObject(const std::string &key) {
    if (!IsObject()) rejectMismatchedType("object", GetValueTypeToString());
    const auto it = object_data_->find(key);
    if (it == object_data_->end())
      throw OutOfRangeError("key '" + key + "' not found");
    return it->second;
  }

  /**
   * @brief Returns a const reference to the value stored under `key`.
   * @param key The object key.
   * @throw TypeError if the node is not an object.
   * @throw OutOfRangeError if `key` does not exist.
   */
  const OrdinaryJsonNode &GetAsObject(const std::string &key) const {
    if (!IsObject()) rejectMismatchedType("object", GetValueTypeToString());
    const auto it = object_data_->find(key);
    if (it == object_data_->end())
      throw OutOfRangeError("key '" + key + "' not found");
    return it->second;
  }

  /**
   * @brief Returns a mutable reference to the element at `index`.
   * @param index The array index (0-based).
   * @throw TypeError if the node is not an array.
   * @throw OutOfRangeError if `index` is out of bounds.
   */
  OrdinaryJsonNode &GetAsArray(const size_t index) {
    if (!IsArray()) rejectMismatchedType("array", GetValueTypeToString());
    if (index >= array_data_->size())
      throw OutOfRangeError("index " + std::to_string(index) +
                            " is out of range");
    return array_data_->at(index);
  }

  /**
   * @brief Returns a const reference to the element at `index`.
   * @param index The array index (0-based).
   * @throw TypeError if the node is not an array.
   * @throw OutOfRangeError if `index` is out of bounds.
   */
  const OrdinaryJsonNode &GetAsArray(const size_t index) const {
    if (!IsArray()) rejectMismatchedType("array", GetValueTypeToString());
    if (index >= array_data_->size())
      throw OutOfRangeError("index " + std::to_string(index) +
                            " is out of range");
    return array_data_->at(index);
  }

  // Scalar accessors ---------------------------------------------------------
  /**
   * @brief Returns a mutable reference to the string value.
   * @throw TypeError if the node is not a string.
   */
  JsonStringType &GetAsString() {
    if (!IsString()) rejectMismatchedType("string", GetValueTypeToString());
    return *string_data_;
  }

  /**
   * @brief Returns a const reference to the string value.
   * @throw TypeError if the node is not a string.
   */
  const JsonStringType &GetAsString() const {
    if (!IsString()) rejectMismatchedType("string", GetValueTypeToString());
    return *string_data_;
  }

  /**
   * @brief Returns a mutable reference to the integer value.
   * @throw TypeError if the node is not an integer.
   */
  int64_t &GetAsInteger() {
    if (!IsInteger()) rejectMismatchedType("integer", GetValueTypeToString());
    return integer_data_;
  }

  /**
   * @brief Returns a const reference to the integer value.
   * @throw TypeError if the node is not an integer.
   */
  const int64_t &GetAsInteger() const {
    if (!IsInteger()) rejectMismatchedType("integer", GetValueTypeToString());
    return integer_data_;
  }

  /**
   * @brief Returns a mutable reference to the double value.
   * @throw TypeError if the node is not a double.
   */
  double &GetAsDouble() {
    if (!IsDouble()) rejectMismatchedType("double", GetValueTypeToString());
    return double_data_;
  }

  /**
   * @brief Returns a const reference to the double value.
   * @throw TypeError if the node is not a double.
   */
  const double &GetAsDouble() const {
    if (!IsDouble()) rejectMismatchedType("double", GetValueTypeToString());
    return double_data_;
  }

  /**
   * @brief Returns a mutable reference to the bool value.
   * @throw TypeError if the node is not a bool.
   */
  bool &GetAsBool() {
    if (!IsBoolean()) rejectMismatchedType("boolean", GetValueTypeToString());
    return boolean_data_;
  }

  /**
   * @brief Returns a const reference to the bool value.
   * @throw TypeError if the node is not a bool.
   */
  const bool &GetAsBool() const {
    if (!IsBoolean()) rejectMismatchedType("boolean", GetValueTypeToString());
    return boolean_data_;
  }

  /**
   * @brief Returns the concrete value type held by this node.
   * @return A `NodeValueTypeEnum` enumerator.
   */
  NodeValueTypeEnum GetValueType() const { return value_type_; }

  /**
   * @brief Returns the value type as a human-readable string.
   * @return "object", "array", "string", "integer", "double", "bool", "null",
   *         or "unknown".
   */
  std::string GetValueTypeToString() const;

  /**
   * @brief Serializes this node to a compact JSON string.
   * @return The JSON representation.
   * @throw TypeError if the node is valueless (kUnknown).
   */
  std::string Stringify() const;

 private:
  union {
    JsonObjectType *object_data_{};
    JsonArrayType *array_data_;
    JsonStringType *string_data_;
    int64_t integer_data_;
    double double_data_;
    bool boolean_data_;
    void *null_data_;
  };

  NodeValueTypeEnum value_type_;

  /// Releases the owned resource (if any) and resets the node to kUnknown.
  void clearResource() {
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
    value_type_ = NodeValueTypeEnum::kUnknown;
  }

  /**
   * @brief Deep-copies `other` into this node.
   * @param other The source node.
   *
   * The target must be empty (kUnknown) or have been cleared with
   * `ClearResource()` first. Heap-backed values (object/array/string) are
   * duplicated so the two nodes share no memory.
   */
  void copyFrom(const OrdinaryJsonNode &other) {
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

  /**
   * @brief Steals the contents of `other` into this node.
   * @param other The source node; left valueless (kUnknown) afterwards.
   *
   * The target must be empty (kUnknown) or have been cleared with
   * `ClearResource()` first.
   */
  void moveFrom(OrdinaryJsonNode &other) noexcept {
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
    other.value_type_ = NodeValueTypeEnum::kUnknown;
  }

  static void rejectMismatchedType(const std::string &expected_type,
                                   const std::string &actual_type);

  void stringifyTo(std::ostream &os) const;
};

namespace internal {
using JsonObjectType = std::map<std::string, OrdinaryJsonNode>;
using JsonArrayType = std::vector<OrdinaryJsonNode>;
using JsonStringType = std::string;
}  // namespace internal

/**
 * @brief Parses a JSON string into a node tree.
 * @param value The JSON text to parse.
 * @return The root node of the parsed tree.
 * @throw ParseError if `value` is not a single valid JSON value (message
 *        includes the 0-based byte offset via `ParseError::byte()`).
 */
OrdinaryJsonNode Parse(const std::string &value);

/**
 * @brief Serializes `value` to a compact JSON string.
 * @param value The node to serialize.
 * @return The JSON representation.
 * @throw TypeError if `value` is valueless (kUnknown).
 */
std::string Stringify(const OrdinaryJsonNode &value);
}  // namespace ordinaryjson

#endif  // ORDINARYJSON_ORDINARY_JSON_HPP_
