# SDF-YANG-Converter

This converter is work in progress. It implements the mapping described in [draft-kiesewalter-asdf-yang-sdf-00](https://www.ietf.org/archive/id/draft-kiesewalter-asdf-yang-sdf-00.html). Both conversion directions (YANG->SDF and SDF->YANG) are finished. Round trips are possible, this feature is not fully implemented though. Only `.sdf.json` is supported as file format for SDF models. For details concerning the conversion please refer to the tables below. Visit the [SDF-YANG-Converter playground](http://sdf-yang-converter.org/) for a demo of the converter. Find conversion examples at [this repository](https://github.com/jkiesewalter/sdf-yang-converter-examples).

Prerequisites:
* A copy of the [YANG GitHub repository](https://github.com/YangModels/yang) is needed to load the context of a YANG file
* [libyang](https://github.com/CESNET/libyang) needs to be installed to parse YANG files
* [nlohmann/json](https://github.com/nlohmann/json) needs to be installed to parse JSON files
* [nlohmann/json-shema-validator](https://github.com/pboettch/json-schema-validator) needs to be installed to validate resulting SDF JSON files with a JSON schema (in this case the validation schema from the [SDF Internet Draft](https://www.ietf.org/archive/id/draft-ietf-asdf-sdf-05.html) is used)

Compile the code with `make`. Run the converter with `./converter -f path/to/input/file [- o path/to/output/file] [-c path/to/yang/repo]`, e.g. `./converter -f ./yang/standard/ietf/RFC/ietf-l2vpn-svc.yang -c ./yang` for conversion from YANG to SDF. If no output file name is provided, the output file will be named after the input model.

A doxygen documentation can be generated directly from the source code by executing `doxygen Doxyfile` (requires doxygen). Afterwards, open `documentation/html/index.html` in your preferred browser for the HTML version of the documentation.

## Conversion table YANG->SDF

|YANG statement|translated to SDF|done?|problems/remarks|
|-|-|-|-|
|module|SDF model|done||
|submodule (included)|Integrated into SDF model of supermodule|done|
|submodule (on its own)|SDF model|done|
|typedef|sdfData definition, sdfRef where it is used|done||
|type (referring to built-in type)|type with other data qualities (e.g. default) if necessary|done|
|type (referring to a typedef)|sdfRef to the corresponding sdfData element|done|
|container (on top-level)|sdfObject|done||
|container (one level below top-level)|sdfProperty of type object\* (compound-type)|done||
|container (on any other level)|property\* (type object\*) of the 'parent' definition of type object\* (compound-type)|done||
|leaf (on top-level and one level below)|sdfProperty (type integer/number/boolean/string)|done|
|leaf (on any other level)|property\* (type integer/number/boolean/string) of the 'parent' definition of type object\* (compound-type)|done|
|leaflist (on top-level and one level below)|sdfProperty of type array|done||
|leaflist (on any other level)|property\* (type array) of the 'parent' definition of type object\* (compound-type)|done||
|list (on top-level and one level below)|sdfProperty of type array with items of type object\* (compound-type)|done||
|list (on any other level)|property\* (type array with items of type object\* (compound-type)) of the 'parent' definition of type object* (compound-type)|done||
|choice|sdfChoice|done|
|case (belonging to choice)|Element of the sdfChoice|done|
|grouping|sdfData of compound-type (type object\*) at the top level which can then be referenced|done|
|uses|sdfRef to the sdfData corresponding to the referenced grouping|done|
|rpc|sdfAction at the top-level of the SDF model|done|
|action|sdfAction of the sdfObject corresponding to a container the action is a descendant node to|done|YANG actions belong to a specific container but containers are not always translated to sdfObjects. Only sdfObjects can have sdfActions, though. The affiliation of the action to the container can thus not always be represented in SDF as of now.|
|notification|sdfEvent|done||
|anydata|not converted|(done)||
|anyxml|not converted|(done)||
|augment|Augments are applied directly in libyang and thus do not need to be translated. There is a note in the description though.|done|
|identity|sdfData definition, sdfRef where it is used|done||
|extension|not converted|(done)|Extensions are used to extend the YANG syntax by a new statement. This can perhaps not be translated into SDF.|
|feature / if-feature| The dependency is mentioned in the description of the element | done | if-feature is used to mark nodes as dependent on whether or not a feature is given. This cannot be tranlsated functionally as of now (?).|
|deviation|Apply deviation directly and state that the model deviates from the standard in the description of the top-level element|||
|config (of a container that was converted to sdfObject)|writable true for all elements in the sdfObject that can be marked as writable (i.e. that use the data qualities)|done|Containers in YANG can be marked with config but sdfObjects (which containers can be converted to) cannot be marked as writable.|
|config (of any other element)|writable true|done||
|status|Mentioned in the description|done|
|reference|Mentioned in the description|done|
|revisions|First revision is tranlated to version of SDF|done|Ignore the other revisions?|
|import|The module that the import references is converted (elements can now be referenced by sdfRef) and its prefix and namespace are added the to namespace section|done|
|when/must|Mentioned in the description|(done)|JSON<->XPATH query language in progress|
|type binary|Type string, sdfType byte-string|done||
|type empty|Type object\* (compound type) with empty properties|done||
|type bits|Type object\* (compound type) with one property for each bit|done||
|type union|sdfChoice over the types of the union|done||


## Conversion table SDF->YANG

|SDF statement|translated to YANG|done?|problems/remarks|
|-|-|-|-|
|sdfProduct|not converted|(done)|Will sdfProduct even exist in future SDF versions?|
|sdfThing|container|done||
|sdfObject|container|done||
|sdfProperty (type integer/number/boolean/string)|leaf node|done|
|sdfProperty (type array with items of type integer/number/boolean/string)|leaf-list node|done|When the resulting leaf-list has one or more default values the libyang parser complains although I think that should be valid.|
|sdfProperty (type array with items of type object (compound-type))|list node|done|
|sdfProperty (type object (compound-type))|container node|done|
|sdfAction (at the top-level, __not__ part of an sdfObject)|RPC node|done||
|sdfAction (inside of an sdfObject)|action node as child node to the container corresponding to the sdfObject|done|
|sdfEvent with sdfOutputData of type integer/number/boolean/string/array|notification node with child nodes that were translated like sdfProperty|done||
|sdfEvent with sdfOutputData of type object|notification node with child nodes corresponding to the object properties\* (translated like sdfProperty)|done||
|sdfInputData/sdfOutputData (type integer/number/boolean/string/array) of an sdfAction|input/output node with child nodes that were translated like sdfProperty|done||
|sdfInputData/sdfOutputData (type object) of an sdfAction|input/output node with child nodes corresponding to the object properties\* (translated like sdfProperty) |done||
|sdfData (type integer/number/boolean/string)|typedef|done|
|sdfData (type array with items of type integer/number/boolean/string)|grouping node with leaf-list child node|done|Added level of grouping causes in-equivalence of instances in round-tripped models|
|sdfData (type array with items of type object (compound-type))|grouping node with list child node|done|see above|
|sdfData (type object (compound-type))|grouping node|done||
|sdfRef (referenced definition was converted to typedef)|type is set to the typedef corresponding to the sdfData definition|done|
|sdfRef (referenced definition was converted to leaf or leaf-list node)|leafref|done|
|sdfRef (referenced definition was converted to grouping node)|uses node (and refine if necessary)|done||
|sdfRef (to sdfData of type array with items of type object (compound-type))|uses node (and refine if necessary)|done|Refines relate to the node type of the refined node. If sdfRef references an sdfData element that also contains an sdfRef the translation will be a uses of a grouping with a uses. If an sdfRef to an sdfData element with another sdfRef is refined with a minItems statement for example that cannot be tranlated directly since uses nodes do not have the min-elements statement.|
|sdfRef (to sdfProperty of type object)|uses node (and refine if necessary). Create a grouping that replaces the container the sdfProperty was translated to.|done||
|sdfRef (to sdfProperty of type array with items of type object)|uses node (and refine if necessary). Create a grouping containing the list that the sdfProperty was translated to.|done|see above|
|sdfRef (to property\* of 'parent' of compound-type/ or array item constraint)|Depends on what the property\* was converted to|done|
|sdfChoice|Choice node with one case node for each alternative of the sdfChoice, each alternative is converted like sdfProperty|done|If the sdfChoice only contains different types it could also be translated to YANG type union (of those different types). YANG choices can only have default cases, so how should default values of simple types be translated?|
|sdfRequired|Set the mandatory statement of the corresponding leaf/choice (which are the only nodes that can explicitely be marked as mandatory). If the corresponding node is not a leaf/choice make the first descendant that is a leaf/choice mandatory|done||

\* please note that an *(object) property* is not the same as sdfProperty and *type object* is not the same as sdfObject
