# SDF-YANG-Converter

* currently, only the direction YANG->SDF is (partly) implemented
* run with './converter path/to/input/file path/to/output/file path/to/yang/repo', e.g. './converter yang/standard/ietf/RFC/ietf-l2vpn-svc.yang test.sdf.json ./yang' for conversion from yang to sdf (currently, only sdf.json is supported as file format for sdf)
* a copy of the [YANG GitHub repository](https://github.com/YangModels/yang) is needed to load the context of a YANG file
* [libyang](https://github.com/CESNET/libyang) is needed to parse YANG files
* [nlohmann/json](https://github.com/nlohmann/json) is needed to parse JSON files
* compile with 'make' or 'g++ converter.cpp sdf.cpp -o converter -lyang'

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

|SDF statement|translated to YANG|
|-|-|
|sdfProduct?||
|sdfThing|module on highest level, container or grouping otherwise|
|sdfObject|module on highest level, container or grouping otherwise|
|sdfProperty|leaf; leaf-list for JSON type array|
|sdfAction|rpc / action with leafs for each sdfData quality; leafref for each sdfInputData, sdfRequiredInputData and sdfOutputData|
|sdfEvent|notification with leafs for each sdfData quality; leafref for each sdfOutputData|
|sdfData|leaf; leaf-list for JSON type array; typedef?|
|sdfRef|leafref / reference to typedef?|
|||
