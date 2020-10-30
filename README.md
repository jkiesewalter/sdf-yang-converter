# SDF-YANG-Converter

* run with './converter path/to/input/file path/to/output/file path/to/yang/repo', e.g. './converter yang/standard/ietf/RFC/ietf-l2vpn-svc.yang test.sdf.json ./yang' for conversion from yang to sdf (currently, only sdf.json is supported as file format for sdf)
* a copy of the YANG GitHub repository is needed to load the context of a YANG file
* libyang is used to parse yang files
* nlohmann/json is used to parse json files
* compile with 'g++ converter.cpp sdf.cpp -o converter -lyang' (Makefile is useless as of now)

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
|choice|tba|
|anydata|tba|
|anyxml|tba|
|grouping|sdfThing|
|uses|tba|
|rpc / action|sdfThing with sdfObject with sdfAction; inputs and outputs from YANG are translated to sdfData and added to the sdfAction as sdfRefs if they are leaf nodes; if inputs/outputs are not leaf nodes they are translated to sdfThings|
|notification|sdfThing with sdfObject with sdfEvent|
|augment|tba|
|identity|sdfObject "identities" with one sdfData element for each *identity*; sdfRef where it is used|
|extension|tba|
|feature|tba|
|if-feature|tba|
|deviation|tba|
|config|tba|
|status|tba|
|description|description of the top-level sdfThing|
|reference|tba|
|when|tba|


## Direction SDF->YANG
