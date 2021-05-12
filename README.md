# SDF-YANG-Converter

This converter is work in progress. Currently, both directions (YANG->SDF and SDF->YANG) are roughly finished. Only `.sdf.json` is supported as file format for SDF models. For details concerning the conversion please refer to the tables below.

Prerequisites:
* A copy of the [YANG GitHub repository](https://github.com/YangModels/yang) is needed to load the context of a YANG file
* [libyang](https://github.com/CESNET/libyang) needs to be installed to parse YANG files
* [nlohmann/json](https://github.com/nlohmann/json) needs to be installed to parse JSON files
* [nlohmann/json-shema-validator](https://github.com/pboettch/json-schema-validator) needs to be installed to validate resulting SDF JSON files with a JSON schema (in this case the validation schema from the [SDF Internet Draft](https://www.ietf.org/archive/id/draft-ietf-asdf-sdf-05.html) is used)

Compile the code with `make`. Run the converter with `./converter -f path/to/input/file [- o path/to/output/file] [-c path/to/yang/repo]`, e.g. `./converter -f ./yang/standard/ietf/RFC/ietf-l2vpn-svc.yang -c ./yang` for conversion from YANG to SDF. If no output file name is provided, the output file will be named after the input model.

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


## Conversion table SDF->YANG

|SDF statement|translated to YANG|done?|problems/remarks|
|-|-|-|-|
|sdfProduct|module on highest level, container otherwise||Will sdfProduct even exist in future SDF versions?|
|sdfThing|module on highest level, container otherwise|done||
|sdfObject|module on highest level, container otherwise|done||
|sdfProperty (type integer/number/boolean/string)|leaf|done|
|sdfProperty (type array with items of type integer/number/boolean/string)|leaf-list|done|When the resulting leaf-list has one or more default values the libyang parser complains although I think that should be valid.|
|sdfProperty (type array with items of type object (compound-type))|list|done|
|sdfProperty (type object (compound-type))|container|done|
|sdfAction (of a sdfObject that is __not__ part of a sdfThing)|RPC|done||
|sdfAction (of a sdfObject that __is__ part of a sdfThing)|action of corresponding container|
|sdfEvent with sdfOutputData of type integer/number/boolean/string/array|notification with child nodes that were translated like sdfProperty|done||
|sdfEvent with sdfOutputData of type object|notification with child nodes corresponding to the object properties\* (translated like sdfProperty)|done||
|sdfInputData/sdfOutputData (type integer/number/boolean/string/array) of an sdfAction|input/output with child nodes that were translated like sdfProperty|done||
|sdfInputData/sdfOutputData (type object) of an sdfAction|input/output with child nodes corresponding to the object properties\* (translated like sdfProperty) |done||
|sdfData (type integer/number/boolean/string)|typedef|done|
|sdfData (type array with items of type integer/number/boolean/string)|grouping with leaf-list|done|
|sdfData (type array with items of type object (compound-type))|grouping  with list|done|
|sdfData (type object (compound-type))|grouping with container|done|
|sdfRef (to sdfData of type integer/number/boolean/string|type is set to the typedef corresponding to the sdfData element|done|
|sdfRef (to sdfProperty of type integer/number/boolean/string or array with items of the aforementioned types)|leafref|done|
|sdfRef (to sdfData of type object)|uses (and refine if necessary)|done||
|sdfRef (to sdfData of type array with items of type object (compound-type))|uses (and refine if necessary) that replaces the node corresponding to the element the sdfRef belongs to|done|Refines relate to the node type of the refined node. If sdfRef references an sdfData element that also contains an sdfRef the translation will be a uses of a grouping with a uses. If an sdfRef to an sdfData element with another sdfRef is refined with a minItems statement for example that cannot be tranlated directly since uses nodes do not have the min-elements statement.|
|sdfRef (to sdfProperty of type object)|Uses (and refine if necessary). Create a grouping that replaces the container the sdfProperty was translated to.|done||
|sdfRef (to sdfProperty of type array with items of type object)|Uses (and refine if necessary) that replaces the node corresponding to the element the sdfRef belongs to. Create a grouping containing the list that the sdfProperty was translated to.|done|see above|
|sdfRef (to property of sdfData/array item constraint of type object (compound-type))|depends on what the property was converted to|done|
|sdfChoice|choice with one case for each element of the sdfChoice; each element is translated like a sdfProperty|done|If the sdfChoice only contains different types it could also be translated to YANG type union (of those different types). YANG choices can only have default cases, so how should default values of simple types be translated?|
|sdfRequired|Set the mandatory statement of the corresponding leaf/choice (which are the only nodes that can explicitely be marked as mandatory). If the corresponding node is not a leaf/choice make the first descendant that is a leaf/choice mandatory|done||

\* please note that an *(object) property* is not the same as sdfProperty and *type object* is not the same as sdfObject
