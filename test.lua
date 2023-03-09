local ffi = require("luaffi")
local lib = ffi.open("mod/mod")

print(ffi.call(lib:getAddress("add"), 1, 2)) -- 3
print(lib:call("add", 2, 3)) -- 5
