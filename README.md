# SDF-YANG-Converter

* libyang is used to parse yang files
* nlohmann/json is used to parse json files
* The YANG GitHub repository is needed to load the context of a YANG file
* compile with 'g++ converter.cpp sdf.cpp -o converter -lyang' (Makefile is useless as of now)
* run with './converter path/to/input/file path/to/output/file', e.g. './converter yang/standard/ietf/RFC/ietf-l2vpn-svc.yang test.sdf.json' for conversion from yang to sdf (currently, only sdf.json is supported as file format for sdf)

## Direction YANG->SDF

|YANG statements|translated to SDF|
|-|-|
|module|sdfThing|
|submodule|sdfThing|
|typedef|sdfObject "typedefs" with one sdfData element for each *typedef*; sdfRef where it is used|
|type|type and other data qualities or sdfRef (for derived types) of sdfProperty or sdfData|
|container|sdfThing|
|leaf|sdfObject with one (if not all sibling nodes are leafs) or multiple (if all siblings are leafs) sdfProperties|
|leaflist|sdfObject with one (if not all sibling nodes are leafs) or multiple (if all siblings are leafs) sdfProperties of type array|
|list|???|
|choice||
|anydata||
|anyxml||
|grouping|sdfThing|
|uses||
|rpc / action|sdfThing with sdfObject with sdfAction(s)|
|notification|sdfThing with sdfObject with sdfEvent(s)|
|augment||
|identity|sdfObject "identities" with one sdfData element for each *identity*; sdfRef where it is used|
|extension||
|feature||
|if-feature||
|deviation||
|config||
|status||
|description|description of the top-level sdfThing|
|reference||
|when||


## Direction SDF->YANG