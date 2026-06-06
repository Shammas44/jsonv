# Supported features

## Validation Keywords (Core)

These keywords apply restrictions to the data instance.

## Numeric Keywords (for `number` and `integer`)

These keywords are used to constrain the size and divisibility of numeric instances.

- [x] `multipleOf`: Value is valid only if it is a multiple of the value of this keyword. 
- [x] `maximum`: The instance must be **less than or equal to** this value.
- [x] `exclusiveMaximum`: The instance must be **strictly less than** this value.
- [x] `minimum`: The instance must be **greater than or equal to** this value.
- [x] `exclusiveMinimum`: The instance must be **strictly greater than** this value.

## String Keywords (for `string`)

These keywords constrain the length and content of string instances.

- [x] `maxLength`: Value must have a length **less than or equal to** this value.
- [x] `minLength`: The string must have a length **greater than or equal to** this value.
- [x] `pattern`: The string instance must match the regular expression defined by this value.
- [ ] `format`: Used to convey the semantic meaning or expected format of the string.

## Array Keywords (for `array`)

These keywords constrain the number of items and the schemas of items within an array instance.

- [x] `maxItems`: Array must have a number of items <= to this value.
- [x] `minItems`: Array must have a number of items >= to this value.
- [x] `items` Defines the schema for the items in the array.
- [x] `uniqueItems`: If `true`, all items in the array must be unique.
- [x] `contains`: Array is valid only if **at least one** of its items validates against schema.

## Object Keywords (for `object`)

These keywords constrain the properties, size, and structure of object instances.

- [x] `maxProperties`: Object must have a number of properties <= to this value.
- [x] `minProperties`: The object must have a number of properties >= to this value.
- [x] `required`: A list of property names that **must be present** in the object instance.
- [x] `properties`: An object where keys are property names and values are schemas.
- [ ] `patternProperties`: Similar to `properties`, but keys are regular expressions.
- [x] `additionalProperties`:  Defines the schema for any properties **not explicitly listed**.
- [x] `propertyNames`: The name of every property in the object must validate against this schema

## Keyword Dependency Keywords

These relate to requiring the presence of certain properties based on others:

* `dependentRequired`: Requires a set of properties to be present if a specified property is present.
* `dependentSchemas`: Requires a schema to be validated against if a specified property is present.

## Structural Keywords (Subschemas)

These keywords define how subschemas are applied to the data instance.

- [x] `type`: Defines the expected **data type**.
- [x] `not`: data must **NOT** be valid against the subschema defined here.
- [x] `allOf`: data must be valid against **ALL** of the subschemas listed in the array.
- [x] `anyOf`: data must be valid against **ANY** of the subschemas listed in the array.
- [x] `oneOf`: data must be valid against **EXACTLY ONE** of the subschemas listed in the array.
- [x] `if`, `then`, `else`: Conditional application of schemas.

## Annotation and Documentation Keywords

These keywords don't affect validation but provide metadata about the schema itself.

* `title`
* `description`
* `default`
* `deprecated`
* `readOnly`
* `writeOnly`
* `examples`

## Schema Identification and Reusability Keywords

These keywords are used for defining, referencing, and reusing schemas.

- [ ] `$schema`: Declares which version of the JSON Schema specification the schema conforms to.
- [ ] `$id`: Defines a base URI for the schema.
- [ ] `$defs`: Used to define subschemas that can be referenced internally.
- [ ] `$ref`: References a schema located at a specific URI (or fragment).
- [ ] `$dynamicRef`, `$dynamicAnchor`: Used for dynamic recursion (Draft 2020-12).
- [ ] `$comment`: For adding notes that implementations should ignore.
- [ ] `$anchor`: Defines a local fragment identifier.
- [ ] `$vocabulary`: Declares which vocabularies (sets of keywords) are used (Draft 2020-12).

## Grammar

- `JSON` $\rightarrow$ Object
- `Value` $\rightarrow$ Object | Array | String | Number | "true" | "false" | "null"
- `Object` $\rightarrow$ "{" Members "}" | "{ }"
- `Members` $\rightarrow$ Pair | Pair "," Members
- `Pair` $\rightarrow$ String ":" Value
- `Array` $\rightarrow$ "[" Elements "]" | "[ ]"
- `Elements` $\rightarrow$ Value | Value "," Elements
