# SDF-YANG-Converter

This converter is work in progress. Currently, the direction YANG->SDF is (roughly) finished, SDF->YANG is implemented in part. Only `.sdf.json` is supported as file format for SDF models. For details concerning the conversion please refer to the tables below.

Prerequisites:
* A copy of the [YANG GitHub repository](https://github.com/YangModels/yang) is needed to load the context of a YANG file
* [libyang](https://github.com/CESNET/libyang) needs to be installed to parse YANG files
* [nlohmann/json](https://github.com/nlohmann/json) needs to be installed to parse JSON files
* [nlohmann/json-shema-validator](https://github.com/pboettch/json-schema-validator) needs to be installed to validate resulting SDF JSON files with a JSON schema (in this case the validation schema from the [SDF Internet Draft](https://www.ietf.org/archive/id/draft-ietf-asdf-sdf-05.html) is used)

Compile the code with `make`. Run the converter with `./converter -f path/to/input/file [- o path/to/output/file] -c path/to/yang/repo`, e.g. `./converter -f yang/standard/ietf/RFC/ietf-l2vpn-svc.yang -c ./yang` for conversion from YANG to SDF. If no output file name is provided, the output file will be named after the input model.

## Conversion table YANG->SDF

|YANG statement|translated to SDF|done?|problems/remarks|
|-|-|-|-|
|module|sdfOjbect|done||
|submodule (included)|Integrated into sdfObject of super module|done|
|submodule (on its own)|sdfObject|done|
|typedef|One sdfData element for each *typedef*; sdfRef where it is used|done||
|type (simple type)|type and maybe other data qualities|done|
|type (derived type)|sdfRef to the corresponding sdfData element|done|
|container (on top-level)|sdfProperty of type object\* (compound-type)|done||
|container (__not__ on top-level)|property\* (type object\*) of the 'parent' of type object\* (compound-type)|done||
|leaf (on top-level)|sdfProperty (type integer/number/boolean/string)|done|
|leaf (__not__ on top-level)|property\* (type integer/number/boolean/string) of the 'parent' of type object\* (compound-type)|done|
|leaflist (on top-level)|sdfProperty of type array|done||
|leaflist (__not__ on top-level)|property\* (type array) of the 'parent' of type object\* (compound-type)|done||
|list|sdfProperty of type object\* (compound-type)|done||
|choice|sdfChoice|done|
|case (belonging to choice)|Element of the sdfChoice|done|
|grouping|sdfData of compound-type (type object\*) at the top level which can then be referenced|done|
|uses|sdfRef to the sdfData corresponding to the referenced grouping|done|
|rpc|sdfAction of the sdfObject corresponding to the rpc's module|done|
|action|sdfAction of the sdfObject corresponding to the module the action occurs in|done|YANG actions belong to a specific container but containers are not always translated to sdfObjects. Only sdfObjects can have sdfActions, though. The affiliation of the action to the container can thus not always be represented in SDF as of now.|
|notification|sdfEvent|done|
|anydata|tba|
|anyxml|tba|
|augment|augments are applied directly in libyang and thus do not need to be translated|done|
|identity|One sdfData element for each identity; sdfRef where it is used|done||
|extension|-|(done)|Extensions are used to extend the YANG syntax by a new statement. This can perhaps not be translated into SDF.|
|feature / if-feature| The dependency is mentioned in the description of the element | done | if-feature is used to mark nodes as dependent on whether or not a feature is given. This cannot be tranlsated functionally as of now (?).|
|deviation|Apply deviation directly and state that the model deviates from the standard in the description of the top-level element|||
|config|tba|
|status|Mentioned in the description|done|
|reference|Mentioned in the description|done|
|revisions|first revision is tranlated to version of SDF|done|Ignore the other revisions?|
|import|Translate the module that the import references (elements can now be referenced by sdfRef)|done|
|when/must|Mentioned in the description|(done)|JSON<->XPATH query language in progress|
|||

\* please note that *property* is not the same as sdfProperty and *type object* is not the same as sdfObject


## Conversion table SDF->YANG

|SDF statement|translated to YANG|done?|problems/remarks|
|-|-|-|-|
|sdfProduct?|module on highest level, container otherwise|
|sdfThing|module on highest level, container otherwise|
|sdfObject|module on highest level, container otherwise|done||
|sdfProperty (type integer/number/boolean/string)|leaf|done|
|sdfProperty (type array with items of type integer/number/boolean/string)|leaf-list|done|
|sdfProperty (type array with items of type object (compound-type))|list|done|
|sdfProperty (type object (compound-type))|container|done|
|sdfAction (of a sdfObject that is __not__ part of a sdfThing)|RPC|
|sdfAction (of a sdfObject that __is__ part of a sdfThing)|action of corresponding container|
|sdfEvent|notification|
|sdfInputData/sdfOutputData|input/output translated like sdfProperty|
|sdfData (type integer/number/boolean/string)|typedef|done|
|sdfData (type array with items of type integer/number/boolean/string)|grouping with leaf-list|done|
|sdfData (type array with items of type object (compound-type))|grouping  with list|done|
|sdfData (type object (compound-type))|grouping with container|done|
|sdfRef (to sdfData/sdfProperty of type integer/number/boolean/string or array with items of the aforementioned types)|leafref|
|sdfRef (to sdfData of type array/object)|uses|
|sdfRef (to sdfProperty of type object or type array with items of type object)|uses (create a grouping for the container that the sdfProperty was translated to)|
|sdfChoice|choice with one case for each element of the sdfChoice; each element is translated like a sdfProperty|done|If the sdfChoice only contains different types it could also be translated to YANG type union (of those different types)|
