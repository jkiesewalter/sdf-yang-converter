#include <iostream>
#include <fstream>
#include <sstream>
#include <stdio.h>
#include <string>
#include <regex>
#include <math.h>
#include <libyang/libyang.h>
#include <nlohmann/json.hpp>
#include "sdf.hpp"

// for convenience
using json = nlohmann::json;
using namespace std;

map<string, sdfCommon*> typedefs;
map<string, sdfCommon*> identities;
map<string, sdfCommon*> leafs;

struct lys_tpdf stringTpdf = {
        .name = "string",
        .type = {.base = LY_TYPE_STRING}
};
struct lys_tpdf dec64Tpdf = {
        .name = "decimal64",
        .type = {.base = LY_TYPE_DEC64}
};
struct lys_tpdf intTpdf = {
        .name = "integer",
        .type = {.base = LY_TYPE_INT64}
};
struct lys_tpdf booleanTpdf = {
        .name = "boolean",
        .type = {.base = LY_TYPE_BOOL}
};
struct lys_tpdf enumTpdf = {
        .name = "enumeration",
        .type = {.base = LY_TYPE_ENUM}
    };

struct lys_node_container *tmp = new lys_node_container();

/*
 * Remove all quotation marks from a given string
 */
string removeQuotationMarksFromString(string input)
{
    string result = input;
    result.erase(std::remove(result.begin(),
            result.end(), '\"' ), result.end());
    return result;
}

/*
 * Cast a char array to string with NULL being
 * translated to the empty string
 */
string avoidNull(const char *c)
{
    if (c == NULL)
        return "";
    return string(c);
}

/*
 * Recursively generate the path of a node
 */
string generatePath(lys_node *node)
{
    if (node->parent == NULL)
        return "/" + avoidNull(node->name);
    return  generatePath(node->parent) + "/" + avoidNull(node->name);
}

/*
 * For a given leaf node that has the type leafref expand the path
 * given by the leafref so that it does not contain ".." anymore
 * but specifies the full path
 */
string expandPath(lys_node_leaf *node)
{
    if (node->type.base != LY_TYPE_LEAFREF)
    {
        cerr << "expandPath(): node " + avoidNull(node->name)
                + "  has wrong base type" << endl;
        return "";
    }
    string path = avoidNull(node->type.info.lref.path);
    //cout << "???" << path << endl;
    lys_node *parent = (lys_node*)node;
    smatch sm;
    regex all_regex("(\\.\\./)+(.*)");
    regex up_regex("\\.\\./");
    if (regex_match(path, sm, all_regex))
    {
        auto begin_up = sregex_iterator(path.begin(), path.end(), up_regex);
        auto end_up = sregex_iterator();
        int match_up = distance(begin_up, end_up);
        for (int i = 0; i < match_up; i++)
            parent = parent->parent;
        path = generatePath(parent) + "/" + string(sm[sm.size()-1]);
        //cout << "!!!" << path << endl;
    }
    return path;
}
/*
 * Overloaded function
 */
string expandPath(lys_node_leaflist *node)
{
    return expandPath((lys_node_leaf*) node);
}


/*
 * Recursively check all parent nodes for given type
 */
bool someParentNodeHasType(struct lys_node *_node, LYS_NODE type)
{
    struct lys_node *node = _node;
    if (node->parent == NULL)
        return false;
    else if (node->parent->nodetype == type)
        return true;
    else
        return someParentNodeHasType(node->parent, type);
}

/*
 * Parse a given libyang base type into the corresponding
 * json type
 */
jsonDataType parseBaseType(LY_DATA_TYPE type)
{
    if (type == LY_TYPE_BOOL)
        return json_boolean;
    else if (type == LY_TYPE_DEC64)
        return json_number;
    else if (type == LY_TYPE_INT8
            || type == LY_TYPE_UINT8
            || type == LY_TYPE_INT16
            || type == LY_TYPE_UINT16
            || type == LY_TYPE_INT32
            || type == LY_TYPE_UINT32
            || type == LY_TYPE_INT64
            || type == LY_TYPE_UINT64)
        return json_integer;
    // TODO: will all enums be translated to type string?
    else if (type == LY_TYPE_STRING || type == LY_TYPE_ENUM)
        return json_string;
    else
    {
        // error handling?
        //cerr << "The base type " << type << " could not be resolved" << endl;
        return json_type_undef;
    }

    // TODO: what about LY_TYPE_BINARY, LY_TYPE_BITS, LY_TYPE_EMPTY,
    // LY_TYPE_IDENT, LY_TYPE_INST, LY_TYPE_LEAFREF, LY_TYPE_UNKNOWN?
}

LY_DATA_TYPE stringToLType(std::string type)
{
    if (type == "string")
        return LY_TYPE_STRING;
    else if (type == "number")
        return LY_TYPE_DEC64;
    else if (type == "boolean")
        return LY_TYPE_BOOL;
    else if (type == "integer")
        return LY_TYPE_INT64;
    else
        return LY_TYPE_UNKNOWN;
}

LY_DATA_TYPE stringToLType(jsonDataType type)
{
    if (type == json_string)
        return LY_TYPE_STRING;
    else if (type == json_number)
        return LY_TYPE_DEC64;
    else if (type == json_boolean)
        return LY_TYPE_BOOL;
    else if (type == json_integer)
        return LY_TYPE_INT64;
    else
        return LY_TYPE_UNKNOWN;
}

/*
 * Translate the types of a union into a vector
 */
/*
vector<jsonDataType> parseType(struct lys_type *type)
//jsonDataType parseType(struct lys_type *type)
{
    if (type->base == LY_TYPE_UNION)
    {
        vector<jsonDataType> result;
        for (int i = 0; i < type->info.uni.count; i++)
            result.push_back(parseBaseType(type->info.uni.types[i].base));
        return result;
    }
    else
        return {parseBaseType(type->base)};
}
*/

/*
 * Extract the type from a given lys_type struct as string
 */
string parseTypeToString(struct lys_type *type)
{
    if (type->base == LY_TYPE_BOOL)
        return "boolean";
    else if (type->base == LY_TYPE_DEC64)
        return "number";
    else if (type->base == LY_TYPE_INT8
            || type->base == LY_TYPE_UINT8
            || type->base == LY_TYPE_INT16
            || type->base == LY_TYPE_UINT16
            || type->base == LY_TYPE_INT32
            || type->base == LY_TYPE_UINT32
            || type->base == LY_TYPE_INT64
            || type->base == LY_TYPE_UINT64)
        return "integer";
    // TODO: will all enums be translated to type string?
    else if (type->base == LY_TYPE_STRING || type->base == LY_TYPE_ENUM)
        return "string";
    else
    {
        // error handling?
        //cerr << "The type " << type->der->name << " could not be resolved"
        //<< endl;
        return "";
    }

    // TODO: what about LY_TYPE_BINARY, LY_TYPE_BITS, LY_TYPE_EMPTY,
    // LY_TYPE_INST, LY_TYPE_LEAFREF, LY_TYPE_UNKNOWN?
}

/*
 * Translate the default value given by a leaf node as char array
 * into the type that corresponds to the type of the given sdfData element
 */
sdfData* parseDefault(const char *value, sdfData *data)
{
    if (value != NULL)
    {
        if (data->getSimpType() == json_string)
            data->setDefaultString(value);

        else if (value != "")
        {
            if(data->getSimpType() == json_number)
                data->setDefaultNumber(atof(value));

            else if(data->getSimpType() == json_boolean)
            {
                //cout << typeid("true").name() << endl;
                //cout << strcmp(value, "true") << endl;
                if (strcmp(value, "true") == 0)
                    data->setDefaultBool(true);
                else if (strcmp(value, "false") == 0)
                    data->setDefaultBool(false);
            }
            else if(data->getSimpType() == json_integer)
                data->setDefaultInt(atoi(value));
        }
    }
    return data;
}

/*
 * Uses regex to take apart range strings (e.g. "0..1" to 0 and 1)
 */
vector<float> rangeToFloat(const char *range)
{
    cmatch cm;
    regex min_max_regex("(.*)\\.\\.(.*)");
    regex_match(range, cm, min_max_regex);
    return {stof(cm[1].str()), stof(cm[2].str())};

}

const char * floatToRange(float min, float max)
{
    if (isnan(min) && isnan(max))
        return NULL;
    std::string first;
    std::string second;
    if (isnan(min))
        first = "";
    else if (min - roundf(min) != 0)
        first = std::to_string(min);
    else
        first = std::to_string((int)min);

    if (isnan(max))
        second = "";
    else if (max - roundf(max) != 0)
        second = std::to_string(max);
    else
        second = std::to_string((int)max);

    std::string *str = new std::string(first + ".." + second);
    return str->c_str();
}

/*
 *  Information from the given lys_type struct is translated
 *  and set accordingly in the given sdfData element
 */
sdfData* typeToSdfData(struct lys_type *type, sdfData *data)
{
    // if type does not refer to a built-in type
    if (typedefs[type->der->name] != NULL && data->getSimpType() != json_array)
    {
        data->setType(type->der->name);
        data->setReference(typedefs[type->der->name]);
        return data;
    }

    if (type->base == LY_TYPE_IDENT)
    {
        // TODO: only one sdfRef possible, what if there are multiple bases?
        data->setDerType("");
        data->setReference(identities[type->info.ident.ref[0]->name]);
    }

    // TODO:
    // as of now, enums are always converted to string type
    else if (type->base == LY_TYPE_ENUM)
        data->setSimpType(json_string);

    jsonDataType jsonType = data->getSimpType();
    // check for types and translate accordingly
    // number
    if (jsonType == json_number)
    {
        // float cnst = NAN;
        float min = NAN;
        float max = NAN;
        float mult = type->info.dec64.div;
        vector<float> enm = {};
        if (type->info.dec64.range != NULL)
        {
            vector<float> min_max
                = rangeToFloat(type->info.dec64.range[0].expr);
            min = min_max[0];
            max = min_max[1];
        }
        data->setNumberData(NAN, NAN, min, max, mult, enm);
    }

    // string
    else if (jsonType == json_string)
    {
        string cnst = "";
        float minLength = NAN;
        float maxLength = NAN;
        jsonSchemaFormat format = json_format_undef;
        string pattern = "";
        vector<string> enum_names = {};

        // TODO: first byte of expr specifies whether to match or invert-match
        // TODO: are the regexs of SDF and YANG compatible?
        if (type->info.str.pat_count > 0)
        {
            pattern = type->info.str.patterns[0].expr;
            // TODO: what about more than one pattern in patterns?
        }

        // TODO: can enums from yang really only be translated to string type?
        if (type->base == LY_TYPE_ENUM)
        {
            for (int i = 0; i < type->info.enums.count; i++)
            {
                enum_names.push_back(type->info.enums.enm[i].name);
                data->setDescription(data->getDescription() + "\n"
                        + type->info.enums.enm[i].name + ": "
                        + type->info.enums.enm[i].dsc);
            }
        }
        else if (type->info.str.length != NULL)
        {
            vector<float> min_max
                = rangeToFloat(type->info.str.length[0].expr);
            minLength = min_max[0];
            maxLength = min_max[1];
        }

        data->setStringData("", "", minLength, maxLength,
                pattern, format, enum_names);
    }

    // boolean
    else if (jsonType == json_boolean)
    {
        vector<bool> enm = {};
        data->setBoolData(false, false, false, false, enm);
    }

    // integer
    else if (jsonType == json_integer)
    {
        //int cnst = -1;
        //bool cnstdef = false;
        float min = NAN;
        float max = NAN;
        vector<int> enm = {};
        if (type->info.num.range != NULL)
        {
            vector<float> min_max
                = rangeToFloat(type->info.num.range[0].expr);
            min = min_max[0];
            max = min_max[1];
        }
        data->setIntData(-1, false, -1, false, min, max, enm);
    }

    else if (jsonType == json_array)
    {
        // TODO: default value (template?)
        float minItems = NAN;
        float maxItems = NAN;
        bool uniqeItems = false;
        string item_type;
        sdfCommon *ref = NULL;
        //if (type->base == LY_TYPE_DER)
        if (typedefs[type->der->name] != NULL)
        {
            item_type = type->der->name;
            ref = typedefs[type->der->name];
        }
        else
            item_type = parseTypeToString(type);
        data->setArrayData(vector<string>(), minItems, maxItems, uniqeItems,
                type->der->name, ref, NAN, NAN);
    }

    return data;
}

/*
 * The information is extracted from the given lys_tpdf struct and
 * a corresponding sdfData object is generated
 */
sdfData* typedefToSdfData(struct lys_tpdf *tpdf)
{
    //vector<jsonDataType> type = parseType(&tpdf->type);
    sdfData *data = new sdfData(avoidNull(tpdf->name), avoidNull(tpdf->dsc),
            parseTypeToString(&tpdf->type));

    // translate units
    string units = "";
    if (tpdf->units != NULL)
    {
        data->setUnits(tpdf->units);
    }

    typeToSdfData(&tpdf->type, data);
    parseDefault(tpdf->dflt, data);

    typedefs[tpdf->name] = data;
    return data;
}

/*
 * The information is extracted from the given lys_node_leaf struct and
 * a corresponding sdfProperty object is generated
 */
sdfProperty* leafToSdfProperty(struct lys_node_leaf *node)
{
    sdfProperty *property = new sdfProperty(avoidNull(node->name),
            avoidNull(node->dsc),
            parseBaseType(node->type.base));

    if (node->type.base == LY_TYPE_LEAFREF)
    {
        property->setType("");
        //cout << node->type.info.lref.path << endl;
        //cout << expandPath(node) << endl;
        property->setReference(leafs[expandPath(node)]);
    }
    else
    {
        typeToSdfData(&node->type, property);
        // save reference to leaf for ability to convert leafref-type
        leafs[generatePath((lys_node*)node)] = property;
    }

    if (node->units != NULL)
    {
        property->setUnits(node->units);
    }
    parseDefault(node->dflt, property);

    // TODO: keyword mandatory -> sdfRequired
    return property;
}

/*
 * The information is extracted from the given lys_node_leaflist struct and
 * a corresponding sdfProperty object is generated
 */
sdfProperty* leaflistToSdfProperty(struct lys_node_leaflist *node)
{
    sdfProperty *property = new sdfProperty(avoidNull(node->name),
            avoidNull(node->dsc), json_array);

    // if the node has type leafref
    if (node->type.base == LY_TYPE_LEAFREF)
    {
        property->setType("");
        property->setArrayData(vector<string>(), NAN, NAN, false,
                "", leafs[expandPath(node)], NAN, NAN);
    }

    else
    {
        // convert all information from the type of the node
        typeToSdfData(&node->type, property);

        // save reference to leaf-list for ability to convert leafref-type
        leafs[generatePath((lys_node*)node)] = property;
    }

    // the number of minimal and maximal items is only valid if at least
    // one of them is not 0
    if (node->min != 0 || node->max != 0)
    {
        property->setMinItems(node->min);
        property->setMaxItems(node->max);
    }
    // translate units
    if (node->units != NULL)
    {
        property->setUnits(node->units);
    }

    // TODO: default values for other types (template?)
    // convert default value of array (if it is given)
    vector<string> dflt;
    for (int i = 0; i < node->dflt_size; i++)
        dflt.push_back(node->dflt[i]);
    property->setDefaultArray(dflt);

    return property;
}

/*
 * The information is extracted from the given lys_node_rpc_action struct and
 * a corresponding sdfAction object is generated
 */
sdfAction* rpcToSdfAction(struct lys_node_rpc_action *node)
{
    sdfAction *action = new sdfAction(avoidNull(node->name),
            avoidNull(node->dsc));

    // convert typedefs into sdfData of the action
    for (int i = 0; i < node->tpdf_size; i++)
        action->addDatatype(typedefToSdfData(&node->tpdf[i]));
    return action;
}

/*
 * The information is extracted from the given lys_node_notif struct and
 * a corresponding sdfEvent object is generated
 */
sdfEvent* notificationToSdfEvent(struct lys_node_notif *node)
{
    sdfEvent *event = new sdfEvent(avoidNull(node->name),
            avoidNull(node->dsc));
    // convert typedefs into sdfData of the event
    for (int i = 0; i < node->tpdf_size; i++)
        event->addDatatype(typedefToSdfData(&node->tpdf[i]));

    return event;
}

/*
 * The information is extracted from the given lys_node_container struct and
 * a corresponding sdfThing object is generated
 */
sdfThing* containerToSdfThing(struct lys_node_container *node)
{
    sdfThing *thing = new sdfThing(avoidNull(node->name),
            avoidNull(node->dsc));
    return thing;
}

/*
 * The information is extracted from the given lys_node_grp struct and
 * a corresponding sdfThing object is generated
 */
sdfThing* groupingToSdfThing(struct lys_node_grp *node)
{
    sdfThing *thing = new sdfThing(avoidNull(node->name),
            avoidNull(node->dsc));

    return thing;
}

/*
 * The information is extracted from the given lys_submodule struct and
 * a corresponding sdfThing object is generated
 */
sdfThing* submoduleToSdfThing(struct lys_submodule *submod)
{
    sdfThing *thing = new sdfThing(avoidNull(submod->name),
            avoidNull(submod->dsc));

    return thing;
}

/*
 * The information is extracted from the given lys_module struct and
 * a corresponding sdfThing object is generated
 */
sdfThing* moduleToSdfThing(const struct lys_module *module)
{

    sdfThing *thing = new sdfThing(avoidNull(module->name),
            avoidNull(module->dsc));
    thing->setInfo(new sdfInfoBlock(avoidNull(module->name)));
    map<string, string> nsMap;
    nsMap[avoidNull(module->prefix)] = avoidNull(module->ns);
    thing->setNamespace(new sdfNamespaceSection(nsMap));

    if (module->tpdf_size > 0)
    {
        sdfObject *tpdfObject = new sdfObject("typedefs",
                "The derived types (typedefs) used in " + thing->getLabel());
        thing->addObject(tpdfObject);
        for (int i = 0; i < module->tpdf_size; i++)
        {
            tpdfObject->addDatatype(typedefToSdfData(&module->tpdf[i]));
        }
    }
    if (module->ident_size > 0)
    {
        sdfObject *tpdfObject = new sdfObject("identities",
                "The identities used in " + thing->getLabel());
        sdfData *ident;
        thing->addObject(tpdfObject);
        for (int i = 0; i < module->ident_size; i++)
        {
            ident
                = new sdfData(module->ident[i].name, module->ident[i].dsc, "");
            // TODO: what if there is more than one base? sdfData only allows
            // for one sdfRef
            for (int j = 0; j < module->ident[i].base_size; j++)
            {
                ident->setReference(identities[module->ident[i].base[j]->name]);
                //cout << module->ident[i].base[j]->name << endl;
            }
            tpdfObject->addDatatype(ident);
            identities[module->ident[i].name] = ident;
        }
    }

    int thingcnt = 0;
    int objcnt = 0;
    sdfThing *parentThing = thing;
    sdfThing *childThing;
    sdfObject *childObject;
    sdfProperty *childProperty;
    sdfAction *childAction;
    sdfEvent *childEvent;
    stack<sdfThing*> branchThings;
    // iterate over all child nodes of the module
    struct lys_node *start0 = module->data;
    struct lys_node *elem0;
    LY_TREE_FOR(start0, elem0)
    {
        parentThing = thing;
        childThing = NULL;
        branchThings = {};

        // for the current child of the module elem0 iterate over all
        // via depth first search (dfs)
        struct lys_node *start1 = elem0;
        struct lys_node *elem1;
        struct lys_node *next1;
        //struct lys_node *last1 = NULL;
        LY_TREE_DFS_BEGIN (start1, next1, elem1)
        //do
        {
            // check node type (container, grouping, leaf, etc.)
            if (elem1->nodetype == LYS_CONTAINER
                    || elem1->nodetype == LYS_INPUT
                    || elem1->nodetype == LYS_OUTPUT)
            {
                // containers are converted to sdfThings
                // (because they can contain more sdfThings)
                childThing = containerToSdfThing((lys_node_container*)elem1);
                parentThing->addThing(childThing);
                thingcnt++;
            }
            else if (elem1->nodetype == LYS_GROUPING)
            {
                // groupings are converted to sdfThings
                // (because they can contain more sdfThings)
                childThing = groupingToSdfThing((lys_node_grp*)elem1);
                parentThing->addThing(childThing);
                thingcnt++;
            }
            else if (elem1->nodetype == LYS_NOTIF)
            {
                // notifications are converted to sdfThings with a
                // corresponding sdfAction
                // (rpcs are not leaf nodes so the sdfThing is needed
                // to allow for further nodes)
                childThing = new sdfThing(avoidNull(elem1->name));
                childObject = new sdfObject(avoidNull(elem1->name));
                childEvent = notificationToSdfEvent((lys_node_notif*)elem1);
                childObject->addEvent(childEvent);
                parentThing->addThing(childThing);
                childThing->addObject(childObject);
                objcnt++;
            }
            else if (elem1->nodetype == LYS_RPC)
            {
                // rpcs/actions are converted to sdfThings with a
                // corresponding sdfAction
                // (rpcs are not leaf nodes so the sdfThing is needed
                // to allow for further nodes)
                childThing = new sdfThing(avoidNull(elem1->name));
                childObject = new sdfObject(avoidNull(elem1->name));
                childAction = rpcToSdfAction((lys_node_rpc_action*)elem1);
                childObject->addAction(childAction);
                parentThing->addThing(childThing);
                childThing->addObject(childObject);
                objcnt++;
            }
            else if (elem1->nodetype == LYS_LEAF)
            {
                // leafs are converted to sdfObjects with a
                // corresponding sdfProperty (or sdfData if they are
                // input/output of an action or event
                childObject = new sdfObject(parentThing->getLabel());
                childProperty = leafToSdfProperty((lys_node_leaf*)elem1);
                //if (last1->nodetype == LYS_INPUT)
                if (someParentNodeHasType(elem1, LYS_INPUT))
                {
                    childObject->setLabel(parentThing->getLabel()
                            + "-data");
                    sdfData *childData =  new sdfData(*childProperty);
                    childObject->addDatatype(childData);
                    childAction->addInputData(childData);
                }
                //else if (last1->nodetype == LYS_OUTPUT)
                else if (someParentNodeHasType(elem1, LYS_OUTPUT))
                {
                    childObject->setLabel(parentThing->getLabel()
                            + "-data");
                    sdfData *childData = new sdfData(*childProperty);
                    childObject->addDatatype(childData);
                    childAction->addOutputData(childData);
                }
                else
                {
                    childObject->setLabel(parentThing->getLabel()
                            + "-properties");
                    childObject->setDescription("Properties of "
                            + parentThing->getLabel());
                    childObject->addProperty(childProperty);
                }
                parentThing->addObject(childObject);
                objcnt++;
            }
            else if (elem1->nodetype == LYS_LEAFLIST)
            {
                // leaf-lists are converted to sdfObjects with one
                // corresponding sdfProperty of type array
                childObject = new sdfObject(parentThing->getLabel(),
                        "Properties of " + parentThing->getLabel());
                childProperty
                    = leaflistToSdfProperty((lys_node_leaflist*)elem1);
                childObject->addProperty(childProperty);
                parentThing->addObject(childObject);
                objcnt++;
            }
            // TODO: if all child nodes of grouping or container are leafs,
            // turn into object instead of thing? Happens automatically
            // because of the name of the sdfObject, right?

            // if the current node has further sibling nodes the current
            // parent sdfThing is saved on a stack to come back to later
            if (elem1->next != NULL)
            {
                branchThings.push(parentThing);
            }
            // if the current node has no child nodes the last sdfThing
            // is loaded from the stack as parent (dfs goes back up)
            // the sdfThing is popped (but pushed back on in the next loop
            // if there are further sibling nodes)
            if (elem1->child == NULL && !branchThings.empty())
            {
                parentThing = branchThings.top();
                branchThings.pop();
            }
            // if the current node does have child nodes the current parent
            // sdfThing is updated to the next sdfThing in line?
            else if (childThing != NULL && elem1->child != NULL)
            {
                parentThing = childThing;
                // parentObject = object would be unnecessary because
                // sdfObjects cannot have objects themselves and
                // the object should be filled on the spot?
            }
            //last1 = elem1;
            LY_TREE_DFS_END(start1, next1, elem1);
        }
    }

    cout << "Created " << thingcnt << " sdfThings"
            << " and " << objcnt << " sdfObjects" << endl;
    return thing;
}

void fillLysType(sdfData *data, struct lys_type *type)
{
    /*
    std::map<uint8_t, const char*> type_map
    {
        {LY_TYPE_BOOL, "boolean"},
        {LY_TYPE_DEC64, "decimal64"},
        {LY_TYPE_STRING, "string"},
        {LY_TYPE_INT64, "int64"},
    };
    */
    type->base = stringToLType(data->getSimpType());
    if (!data->getEnumBool().empty() || !data->getEnumNumber().empty()
            || !data->getEnumString().empty() || !data->getEnumInt().empty())
    {
        type->base = LY_TYPE_ENUM;
        type->der = &enumTpdf;
        int enmSize = 0;
        static std::vector<std::string> enmNames;
        if (!data->getEnumBool().empty())
        {
            std::vector<bool> bools = data->getEnumBool();
            enmSize = bools.size();
            enmNames.resize(enmSize);
            for (int i = 0; i < enmSize; i++)
            {
                enmNames[i] = bools[i] ? "true" : "false";
            }
        }
        else if (!data->getEnumNumber().empty())
        {
            std::vector<float> numbers = data->getEnumNumber();
            enmSize = numbers.size();
            enmNames.resize(enmSize);
            for (int i = 0; i < enmSize; i++)
            {
                enmNames[i] = to_string(numbers[i]);
            }
        }
        else if (!data->getEnumString().empty())
        {
            std::vector<std::string> strings = data->getEnumString();
            enmSize = strings.size();
            enmNames.resize(enmSize);
            for (int i = 0; i < enmSize; i++)
            {
                enmNames[i] = strings[i];
            }
        }
        else if (!data->getEnumInt().empty())
        {
            std::vector<int> ints = data->getEnumInt();
            enmSize = ints.size();
            enmNames.resize(enmSize);
            for (int i = 0; i < enmSize; i++)
            {
                enmNames[i] = to_string(ints[i]);
            }
            cout << "alsdjf"<<endl;
        }
        type->info.enums.count = enmSize;
        static std::vector<lys_type_enum> enms(enmSize);
        for (int i = 0; i < enmSize; i++)
        {
            enms[i].name = enmNames[i].c_str();
            enms[i].value = i;
        }
        type->info.enums.enm = enms.data();
        cout << "here"<< endl;
    }
    else if (type->base == LY_TYPE_BOOL)
    {
        //type->base = LY_TYPE_BOOL;
        type->der = &booleanTpdf;
    }
    else if (type->base == LY_TYPE_DEC64)
    {
        //type->base = LY_TYPE_DEC64;
        type->der = &dec64Tpdf;
        type->info.dec64.dig = 6;
        // TODO: decide number of fraction-digits
        // (std::to_string is always 6)
        type->info.dec64.div = (int)data->getMultipleOf();
        const char * range = floatToRange(data->getMinimum(), data->getMaximum());
        if (range)
        {
            type->info.dec64.range = new lys_restr();
            type->info.dec64.range->expr = range;
        }

    }
    else if (type->base == LY_TYPE_STRING)
    {
        //type->base = LY_TYPE_STRING;
        type->der = &stringTpdf;
        const char * range = floatToRange(data->getMinLength(),
                data->getMaxLength());
        if (range)
        {
            type->info.str.length = new lys_restr();
            type->info.str.length->expr = range;
        }
        if (data->getPattern() != "")
        {
            type->info.str.patterns = new lys_restr[1]();
            type->info.str.pat_count = 1;
            char ack = 0x06; // ACK for match
            std::string *str = new std::string(ack + data->getPattern());
            type->info.str.patterns[0].expr = str->c_str();
        }
    }
    else if (type->base == LY_TYPE_INT64)
    {
        //type->base = LY_TYPE_INT64;
        type->der = &intTpdf;
    }
}

// TODO: choice, list (with arrays???), anydata, uses, case,
// inout (?), augment

struct lys_tpdf * sdfDataToTypedef(sdfData *data, struct lys_tpdf *tpdf)
{
    tpdf->name = data->getLabelAsArray();
    tpdf->dsc = data->getDescriptionAsArray();
    //tpdf->ref = NULL; // optional
    //tpdf->flags = 0; // optional
    //module->tpdf[cnt].ext_size = 0; // optional
    //module->tpdf[cnt].padding_iffsize = 0; // optional
    //module->tpdf[cnt].has_union_leafref = 0; // optional
    //uint8_t pad[3] = {0};
    //std::memcpy(module->tpdf[cnt].padding, pad, 3); // optional
    if (data->getUnits() != "")
        tpdf->units = data->getUnitsAsArray(); // optional

    // type
    fillLysType(data, &tpdf->type);
    tpdf->type.parent = tpdf;

    const char *tmp = data->getDefaultAsCharArray();
    tpdf->dflt = tmp;
    return tpdf;
}

void sdfPropertyToLeaf(sdfProperty *prop, lys_node_leaf *leaf)
{
    leaf->nodetype = LYS_LEAF;
    leaf->name = prop->getLabelAsArray();
    leaf->dsc = prop->getDescriptionAsArray();
    fillLysType(prop, &leaf->type);
    leaf->type.parent = (lys_tpdf*)leaf;
    if (prop->getUnits() != "")
        leaf->units = prop->getUnitsAsArray(); // optional
    leaf->dflt = prop->getDefaultAsCharArray();
}

struct lys_module* sdfObjectToModule(sdfObject *object)
{
    // TODO: write blank yang file to use here and take context from input
    const lys_module *buf_module = lys_parse_path(ly_ctx_new("./yang", 0),
                 "yang/standard/ietf/RFC/ietf-l2vpn-svc.yang", LYS_IN_YANG);

    struct lys_module *module = const_cast<lys_module*>(buf_module);
    //module->version = 0;
    std::cout << module->tpdf[0].type.der->dsc << std::endl;

    module->name = object->getInfo()->getTitleAsArray();
    char *cat = new char[std::strlen(object->getInfo()->getTitleAsArray())
                         + std::strlen(object->getDescriptionAsArray())
                         + 1];
    std::strcpy(cat, object->getInfo()->getTitleAsArray());
    std::strcat(cat, "\n\n");
    std::strcat(cat, object->getDescriptionAsArray());
    std::strcat(cat, "\n\nCopyright: ");
    std::strcat(cat, object->getInfo()->getCopyrightAsArray());
    std::strcat(cat, "\nLicense: ");
    std::strcat(cat, object->getInfo()->getLicenseAsArray());
    std::strcat(cat, "\nVersion: ");
    std::strcat(cat, object->getInfo()->getVersionAsArray());
    module->dsc = cat;
    module->prefix = object->getNamespace()->getNamespacesAsArrays().begin()
                                                                      ->first;
    module->ns = object->getNamespace()->getNamespacesAsArrays().begin()
                                                                      ->second;
    module->org = object->getInfo()->getCopyrightAsArray();
    module->ref = "";
    module->contact = "";
    module->type = 0;

    uint16_t cnt = 0;
    module->tpdf = new lys_tpdf[object->getDatatypes().size()]();
    if (module->tpdf == nullptr)
        cerr << "Error: memory could not be allocated" << endl;
    for (sdfData *i : object->getDatatypes())
    {
        sdfDataToTypedef(i, &module->tpdf[cnt]);
        module->tpdf[cnt].module = module;

        cnt++;
    }
    module->tpdf_size = cnt;

    static struct lys_node_container props = {
            .name = "properties",
            .dsc = "",
            //.ref = NULL,
            //.flags = 0,
            //.ext_size = 0,
            //.iffeature_size = 0,
            //.must_size = 0,
            //.tpdf_size = 0,
            .module = module,
            .nodetype = LYS_CONTAINER,
            //.parent = NULL,
            //.child = NULL,
            //.next = NULL,
            .prev = (lys_node*)&props,
            //.priv = NULL,
            //.when = NULL,
            //.must = NULL,
            //.tpdf = NULL,
            //.presence = NULL
    };
    module->data = (lys_node*)&props;

    std::vector<sdfProperty*> properties = object->getProperties();
    static std::vector<lys_node_leaf> leafs(properties.size());
    for (int i = 0; i < properties.size(); i++)//sdfProperty *i : object->getProperties())
    {
        sdfPropertyToLeaf(properties[i], &leafs[i]);
        leafs[i].module = module;
        leafs[i].parent = (lys_node*)&props;
        if (i < properties.size()-1)
            leafs[i].next = (lys_node*)&leafs[i+1];
        if (i > 0)
            leafs[i].prev = (lys_node*)&leafs[i-1];
    }
    leafs[0].prev = (lys_node*)&leafs[0];
    props.child = (lys_node*)&leafs[0];

    for (sdfAction *i : object->getActions())
    {

    }

    for (sdfEvent *i : object->getEvents())
    {

    }

    module->type = 0;
    module->deviated = 0;
    module->rev_size = 0;
    module->imp_size = 0;
    module->inc_size = 0;
    module->ident_size = 0;
    module->features_size = 0;
    module->augment_size = 0;
    module->deviation_size = 0;
    module->extensions_size = 0;
    module->ext_size = 0;
    //module->data = new lys_node();
    //module->data = NULL;
    return module;
}

/*
 * The information from a sdfThing is transferred into a YANG module
 */
struct lys_module* sdfThingToModule(sdfThing *thing)
{
    struct lys_module *module;
    printf("in sdfThingToModule\n");
    module->version = 0;
    module->name = thing->getLabel().c_str();
    std::cout << thing->getLabel().c_str() << std::endl;
    module->dsc = "oh hi";//thing->getDescription().c_str();
    module->prefix = "ietf";
    module->ns = "stand";
    module->org = "";
    module->ref = "";
    module->contact = "";
    module->type = 0;
    module->deviated = 0;
    module->rev_size = 0;
    module->imp_size = 0;
    module->inc_size = 0;
    module->ident_size = 0;
    module->tpdf_size = 0;
    module->features_size = 0;
    module->augment_size = 0;
    module->deviation_size = 0;
    module->extensions_size = 0;
    module->ext_size = 0;
    //module->data = new lys_node();
    module->data = NULL;
    return module;
}

int main(int argc, const char** argv)
{
    // Executed by calling: sdf_converter input_file output_file

    /*
    // open specified input file
    std::ifstream input_file;
    std::string input_string;
    input_file.open (argv[1]);
    // read contents of input file
    if (input_file)
    {
        std::ostringstream ss;
        ss << input_file.rdbuf();
        input_string = ss.str();
        input_file.close();
    }
    else
    {
        perror("Error opening file");
        return -1;
    }
    */

    // regexs to check file formats
    std::regex yang_regex (".*\\.yang");
    std::regex sdf_json_regex (".*\\.sdf\\.json");

    // check whether input file is a YANG file
    if (std::regex_match(argv[1], yang_regex))
    {
        // TODO: check whether output file has right format
        if (!std::regex_match(argv[2], sdf_json_regex))
        {
            cerr << "Incorrect output file format\n" << endl;
            //return -1;
        }

        // load the required context
        ly_ctx *ctx = ly_ctx_new(argv[3], 0);
        // load the module
        const lys_module *module = lys_parse_path(ctx, argv[1], LYS_IN_YANG);
        //lys_print_path("test123.yang", module, LYS_OUT_YANG, "", 0, 0);

        if (module == NULL)
        {
            cerr << "Module parsing failed" << endl;
            return -1;
        }

        // Convert the module to a sdfThing
        sdfThing *moduleThing = moduleToSdfThing(module);
        // Print the sdfThing into a file (in sdf.json format)
        moduleThing->thingToFile(argv[2]);
        /*
        //TEST
        sdfObject test("test", "");
        sdfEvent teste("e", "...");
        sdfData testd("d", "...", json_string);
        sdfProperty testp("p", "prop");
        sdfAction testa("testa", "blah");
        test.addAction(&testa);
        test.addEvent(&teste);
        test.addProperty(&testp);
        testa.addDatatype(&testd);
        testp.setReference(&testd);
        teste.addOutputData(&testd);
        testa.addInputData(&testd);
        testa.addRequiredInputData(&testd);

        testd.setStringData("yeah");

        test.objectToFile(argv[2]);
        */

        return 0;
    }

    // check whether input file is an SDF file
    if (std::regex_match(argv[1], sdf_json_regex))
    {
        // check whether output file has right format
        if (!std::regex_match(argv[2], yang_regex))
        {
            printf("Incorrect output file format\n");
            //return -1;
        }

        // TODO: fill module
        sdfObject *moduleObject = new sdfObject();
        sdfThing *moduleThing = new sdfThing();
        lys_module *module = new lys_module;
        module->ctx = ly_ctx_new(argv[3], 0);
        module = const_cast<lys_module*>(lys_parse_path(module->ctx,
                        "yang/standard/ietf/RFC/ietf-l2vpn-svc.yang",
                        LYS_IN_YANG));
        if (moduleObject->fileToObject(argv[1]) != NULL)
        {
            cout
            << removeQuotationMarksFromString(moduleObject->objectToString())
            << endl;
            module = sdfObjectToModule(moduleObject);
        }
        else
        {
            moduleThing->fileToThing(argv[1]);
            cout << moduleThing->thingToString() << endl;
            module = sdfThingToModule(moduleThing);
        }
        const lys_module *const_module = const_cast<lys_module*>(module);
        printf("Made it!\n");
        lys_print_path(argv[2], const_module, LYS_OUT_YANG, NULL, 0, 0);

        return 0;
    }

    return -1;
}
