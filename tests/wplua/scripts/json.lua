-- Null
json = Json.Null ()
assert (json:is_null())
assert (json:parse() == nil)
assert (json:get_data() == "null")
assert (json:get_size() == 4)

-- Boolean
json = Json.Boolean (true)
assert (json:is_boolean())
assert (json:parse())
assert (json:get_data() == "true")
assert (json:get_size() == 4)
json = Json.Boolean (false)
assert (json:is_boolean())
assert (not json:parse())
assert (json:get_data() == "false")
assert (json:get_size() == 5)

-- Int
json = Json.Int (3)
assert (json:is_int())
assert (json:parse() == 3)
assert (json:get_data() == "3")
assert (json:get_size() == 1)

-- Float
json = Json.Float(3.14)
assert (json:is_float())
val = json:parse ()
assert (val > 3.13 and val < 3.15)

-- String
json = Json.String ("wireplumber")
assert (json:is_string())
assert (json:parse() == "wireplumber")
assert (json:get_data() == "\"wireplumber\"")
assert (json:get_size() == 13)

-- Array
json = Json.Array { Json.Null (), Json.Null () }
assert (json:is_array())
val = json:parse ()
assert (val[1] == nil)
assert (val[2] == nil)
assert (json:get_data() == "[null, null]")
assert (json:get_size() == 12)

json = Json.Array { true, false }
assert (json:is_array())
val = json:parse ()
assert (val[1])
assert (not val[2])
assert (json:get_data() == "[true, false]")
assert (json:get_size() == 13)

json = Json.Array {1, 2, 3}
assert (json:is_array())
val = json:parse ()
assert (val[1] == 1)
assert (val[2] == 2)
assert (val[3] == 3)
assert (json:get_data() == "[1, 2, 3]")
assert (json:get_size() == 9)

json = Json.Array {1.11, 2.22, 3.33}
assert (json:is_array())
val = json:parse ()
assert (val[1] > 1.10 and val[1] < 1.12)
assert (val[2] > 2.21 and val[2] < 2.23)
assert (val[3] > 3.32 and val[3] < 3.34)

json = Json.Array {"lua", "spa", "json"}
assert (json:is_array())
val = json:parse ()
assert (val[1] == "lua")
assert (val[2] == "spa")
assert (val[3] == "json")
assert (json:get_data() == "[\"lua\", \"spa\", \"json\"]")
assert (json:get_size() == 22)

json = Json.Array {
  Json.Array {
    Json.Object {
      key1 = 1
    },
    Json.Object {
      key2 = 2
    },
  }
}
assert (json:is_array())
assert (json:get_data() == "[[{\"key1\":1}, {\"key2\":2}]]")

-- Object
json = Json.Object {
    key1 = Json.Null(),
    key2 = true,
    key3 = 3,
    key4 = 4.44,
    key5 = "foo",
    key6 = Json.Array {5, 6, 7},
    key7 = Json.Object {
      key_nested1 = "nested",
      key_nested2 = 8,
      key_nested3 = Json.Array {false, true, false}
    }
}
assert (json:is_object())
val = json:parse ()
assert (val.key1 == nil)
assert (val.key2 == true)
assert (val.key3 == 3)
assert (val.key4 > 4.43 and val.key4 < 4.45)
assert (val.key5 == "foo")
assert (val.key6[1] == 5)
assert (val.key6[2] == 6)
assert (val.key6[3] == 7)
assert (val.key7.key_nested1 == "nested")
assert (val.key7.key_nested2 == 8)
assert (not val.key7.key_nested3[1])
assert (val.key7.key_nested3[2])
assert (not val.key7.key_nested3[3])

-- Raw
json = Json.Raw ("[\"foo\", \"bar\"]")
assert (json:is_array())
assert (json:get_data() == "[\"foo\", \"bar\"]")
val = json:parse ()
assert (val[1] == "foo")
assert (val[2] == "bar")

json = Json.Raw ("{\"name\": \"wireplumber\", \"version\": [0, 4, 7]}")
assert (json:is_object())
assert (json:get_data() == "{\"name\": \"wireplumber\", \"version\": [0, 4, 7]}")
val = json:parse ()
assert (val.name == "wireplumber")
assert (val.version[1] == 0)
assert (val.version[2] == 4)
assert (val.version[3] == 7)
