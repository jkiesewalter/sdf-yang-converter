#include <iostream>
#include <fstream>
#include <sstream>
#include <stdio.h>
#include <string>
#include <regex>
#include <math.h>
#include <ctype.h>
#include <algorithm>
#include <libyang/libyang.h>
#include <nlohmann/json.hpp>
#include <nlohmann/json-schema.hpp>
#include "sdf.hpp"

// TODO: are these the correct numbers?
#define MAX_NUM 3.4e+38 // maximal float
#define MAX_INT 2147483647 // maximal integer (64)
#define JSON_MAX_STRING_LENGTH 2097152 // maximal length of a json string

// for convenience
using json = nlohmann::json;
using nlohmann::json_schema::json_validator;
using namespace std;

map<string, sdfCommon*> typedefs;
/* vector of tuples with typedef names and sdfCommons that use them */
vector<tuple<string, sdfCommon*>> typerefs;

map<string, sdfCommon*> identities;
vector<tuple<string, sdfCommon*>> identsLeft;

map<string, sdfCommon*> leafs;
// leftover references:
vector<tuple<string, sdfCommon*>> referencesLeft;

map<string, sdfCommon*> branchRefs;

struct lys_tpdf stringTpdf = {
        .name = "string",
        .type = {.base = LY_TYPE_STRING}
};
struct lys_tpdf dec64Tpdf = {
        .name = "decimal64",
        .type = {.base = LY_TYPE_DEC64}
};
struct lys_tpdf intTpdf = {
        .name = "int64", // TODO: use another int type, e.g. int32?
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
struct lys_tpdf leafrefTpdf = {
        .name = "leafref",
        .type = {.base = LY_TYPE_LEAFREF}
};

struct lys_node_container *tmp = new lys_node_container();

sdfData* nodeToSdfData(struct lys_node *node, sdfObject *object);

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
    if (node == NULL)
    {
        cerr << "generatePath: node is null" << endl;
        return "";
    }
    else if (node->parent == NULL)
        return "/" + avoidNull(node->name);
    return  generatePath(node->parent) + "/" + avoidNull(node->name);
}

/*
 * For a given leaf node that has the type leafref expand the target path
 * given by the leafref so that it does not contain ".." anymore
 * but specifies the full path
 * It is not possible to simply call generatePath on the target in
 * the node info because the target node is not always given
 */
string expandPath(lys_node_leaf *node)
{
    if (node == NULL)
    {
        cerr << "expandPath: node is null" << endl;
        return "";
    }
    if (node->type.base != LY_TYPE_LEAFREF)
    {
        cerr << "expandPath(): node " + avoidNull(node->name)
                + "  has to be of base type leafref" << endl;
        return "";
    }
    string path = avoidNull(node->type.info.lref.path);
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

// TODO: union?
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
 * Uses regex to take apart range strings (e.g. "0..1" to 0 and 1)
 */
vector<float> rangeToFloat(const char *range)
{
    cmatch cm;
    string str(range);

    // Regex to match a single range
    regex min_max_regex("([\\w\\d]+)\\.\\.([\\w\\d]+)");
    if (regex_match(range, cm, min_max_regex))
    {
        string min = cm[1].str();
        string max = cm[2].str();
        if (min != "min" && max != "max")
            return {stof(min), stof(max)};

        if (min != "min" && max == "max")
            return {stof(min), MAX_NUM};

        if (min == "min" && max != "max")
            return {-MAX_NUM, stof(max)};

        // this should never be reached
        cerr << "range values '" + str + "' are invalid" << endl;
        return {};
    }

    // regex to match several range options
    regex mult_ranges_regex("(([\\w\\d]+\\.\\.[\\w\\d]+)\\|*)+");
    if (regex_match(range, mult_ranges_regex))
    {
        std::sregex_iterator iter(str.begin(), str.end(), min_max_regex);
        std::sregex_iterator end;

        vector<float> result = {};
        while (iter != end)
        {
            string match = (*iter)[0].str();
            for (float value : rangeToFloat(match.c_str()))
                result.push_back(value);
            iter++;
        }
        return result;
    }

    // if the string consists of a single number instead of a range
    if (str == "min")
        return {-MAX_NUM, -MAX_NUM};
    if (str == "max")
        return {MAX_NUM, MAX_NUM};

    try
    {
        return {stof(str), stof(str)};
    }
    catch (std::invalid_argument&)
    {
        cerr << "range string '" + str + "' is not a correct range"
                << endl;
    }
    return {};
}

const char * floatToRange(float min, float max)
{
    if (isnan(min) && isnan(max))
        return NULL;
    if (min == max)
    {
        string *str = new string(to_string(min));
        return str->c_str();
    }
    std::string first;
    std::string second;
    if (isnan(min))
        first = "min";
    else if (min - roundf(min) != 0)
        first = std::to_string(min);
    else
        first = std::to_string((int)min);

    if (isnan(max))
        second = "max";
    else if (max - roundf(max) != 0)
        second = std::to_string(max);
    else
        second = std::to_string((int)max);

    std::string *str = new std::string(first + ".." + second);
    return str->c_str();
}

string statusToDescription(uint16_t flags, string dsc)
{
    if ((flags & LYS_STATUS_MASK) == LYS_STATUS_DEPRC)
        return "Status: DEPRECATED\n\n" + dsc;
    else if ((flags & LYS_STATUS_MASK) == LYS_STATUS_OBSLT)
        return "Status: OBSOLETE\n\n" + dsc;
    return dsc;
}

string mustToDescription(lys_restr *must, int size, string dsc)
{
    // TODO: test
    string result = dsc;

    for (int i = 0; i < size; i++)
        result = result + "\n\n" + avoidNull(must[i].dsc)
            + "\nmust (XPath expression): " + avoidNull(must[i].expr);

    return result;
}

string whenToDescription(lys_when *when, string dsc)
{
    if (when)
        return dsc + "\n\n" + avoidNull(when->dsc)
                + "\nwhen (XPath expression): "
                + avoidNull(when->cond);
    return dsc;
}

/*
 *  Information from the given lys_type struct is translated
 *  and set accordingly in the given sdfData element
 */
sdfData* typeToSdfData(struct lys_type *type, sdfData *data)
{
    // TODO: clean up this function
    // TODO: type union, bits, empty?

    if (type->base == LY_TYPE_LEAFREF)
    {
        // what if instead of using the path, the target node would be
        // stored with the corresponding data element somehow?
        data->setType("");
        if (type->info.lref.target)
        {
            referencesLeft.push_back(tuple<string, sdfCommon*>{
               generatePath((lys_node*)type->info.lref.target),
                       (sdfCommon*)data});
        }
        else
        {
            referencesLeft.push_back(tuple<string, sdfCommon*>{
                expandPath((lys_node_leaf*)type->parent), (sdfCommon*)data});
        }
    }
    else if (strcmp(type->der->name, "empty") == 0)
    {
        data->setSimpType(json_boolean);
        data->setConstantBool(true); // TODO: translate like this?
    }
    else if (stringToJsonDType(type->der->name) == json_type_undef
            && data->getSimpType() != json_array
            && type->base != LY_TYPE_ENUM && type->base != LY_TYPE_IDENT)
    {
        //data->setType(json_type_undef); // TODO: do this?
        typerefs.push_back(tuple<string, sdfCommon*>{
           type->der->name, data});

    }
    /*else? */if (type->base == LY_TYPE_IDENT)
    {
        if (type->info.ident.count > 1)
        {
            for (int i = 0; i < type->info.ident.count; i++)
            {
                sdfData *ref = new sdfData(
                        avoidNull(type->info.ident.ref[i]->name),
                        avoidNull(type->info.ident.ref[i]->dsc),
                        json_type_undef);
                if (identities[type->info.ident.ref[i]->name])
                    ref->setReference(
                            identities[type->info.ident.ref[i]->name]);
                else
                {
                    cerr << "typeToSdfData: identity reference is null" << endl;
                    identsLeft.push_back(tuple<string, sdfCommon*>{
                        type->info.ident.ref[i]->name, (sdfCommon*)data});
                }
                // TODO: test
                data->setSimpType(json_object);
                data->addObjectProperty(ref);
            }
        }
        else if (type->info.ident.count < 1)
            cerr << "typeToSdfData: type identity but identity count is 0"
            << endl;
        else
        {
            data->setType(json_type_undef);
            if (identities[type->info.ident.ref[0]->name])
                data->setReference(identities[type->info.ident.ref[0]->name]);
            else
                cerr << "typeToSdfData: identity reference is null" << endl;
        }
    }

    else if (type->base == LY_TYPE_ENUM)
        data->setType(json_string);

    jsonDataType jsonType = data->getSimpType();
    // check for types and translate accordingly
    // number
    if (jsonType == json_number)
    {
        data->setMultipleOf(type->info.dec64.div); // TODO: check / test
        if (type->info.dec64.range != NULL)
        {
            vector<float> min_max
                = rangeToFloat(type->info.dec64.range->expr);

            for (int i = 0; i < min_max.size()-1; i = i+2)
            {
                sdfData *d;
                if (min_max.size() > 2)
                {
                    d = new sdfData("range_option_" + to_string(i/2), "",
                            json_type_undef);
                    data->addChoice(d);
                }
                else
                    d = data;

                if (min_max[i] == min_max[i+1])
                {
                    if (min_max[i] < -MAX_NUM-1)
                        d->setConstantNumber(-MAX_NUM-1);
                    else if (min_max[i] > MAX_NUM)
                        d->setConstantNumber(MAX_NUM);
                    else
                        d->setConstantNumber(min_max[i]);
                }
                else
                {
                    if (min_max[i] >= -MAX_NUM)
                        d->setMinimum(min_max[i]);
                    if (min_max[i+1] < MAX_NUM)
                        d->setMaximum(min_max[i+1]);
                }
            }
        }
    }

    // string
    else if (jsonType == json_string)
    {
        string cnst = ""; // TODO: constant string == pattern?
        //jsonSchemaFormat format = json_format_undef;
        //string pattern = "";

        // TODO: first byte 0x06 means regular match, 0x15 means invert-match
        // TODO: are the regexs of SDF and YANG compatible?
        if (type->info.str.pat_count > 0)
        {
            //pattern = type->info.str.patterns[0].expr;
            //data->setPattern(pattern);
            if (type->info.str.pat_count == 1)
            {
                // the first byte of expr is ignored because it contains
                // information about the match type (regular or invert)
                data->setPattern(type->info.str.patterns[0].expr+1);
                if (type->info.str.patterns[0].dsc
                        || type->info.str.patterns[0].ref)
                    data->setDescription(data->getDescription()
                            + "\n\nPattern: "
                            + avoidNull(type->info.str.patterns[0].dsc) + "\n"
                            + avoidNull(type->info.str.patterns[0].ref));
            }

            else // TODO: patterns are ANDed not ORed so this is bs
            {
                sdfData *choice;
                for (int i = 0; i < type->info.str.pat_count; i++)
                {
                    choice = new sdfData("pattern_option_" + to_string(i), "",
                            json_type_undef);
                    choice->setPattern(type->info.str.patterns[i].expr+1);
                    data->addChoice(choice);
                }
            }
            // TODO: Test
        }

        // TODO: enums in yang can only be translated to string type
        if (type->base == LY_TYPE_ENUM)
        {
            vector<string> enum_names = {};
            for (int i = 0; i < type->info.enums.count; i++)
            {
                enum_names.push_back(type->info.enums.enm[i].name);
                data->setDescription(data->getDescription() + "\n"
                        + type->info.enums.enm[i].name + ": "
                        + type->info.enums.enm[i].dsc);
            }
            data->setEnumString(enum_names);
        }
        else if (type->info.str.length != NULL)
        {
            vector<float> min_max
                = rangeToFloat(type->info.str.length->expr);

            for (int i = 0; i < min_max.size()-1; i = i+2)
            {
                sdfData *d;
                if (min_max.size() > 2)
                {
                    d = new sdfData("length_option_" + to_string(i/2), "",
                            json_type_undef);
                    data->addChoice(d);
                }
                else
                    d = data;

                if (min_max[i] < 0 || min_max[i+1] < 0)
                    cerr << "typeToSdfData: minLength or maxLength < 0" << endl;
                if (min_max[i] > 0)
                    d->setMinLength(min_max[i]);
                if (min_max[i+1] < JSON_MAX_STRING_LENGTH)
                    d->setMaxLength(min_max[i+1]);
            }

            /*if (min_max[0] != -MAX_NUM)
                minLength = min_max[0];
            if (min_max[1] != MAX_NUM)
                maxLength = min_max[1];

            if (minLength < 0 || maxLength < 0)
                cerr << "typeToSdfData: minLength or maxLength < 0" << endl;
        */}

        //data->setStringData("", "", minLength, maxLength,
        //        pattern, format, enum_names);
    }

    // boolean
    else if (jsonType == json_boolean)
    {
        //vector<bool> enm = {};
        data->setBoolData(false, false, false, false);
    }

    // integer
    else if (jsonType == json_integer)
    {
        if (type->info.num.range != NULL)
        {
            vector<float> min_max
                = rangeToFloat(type->info.num.range->expr);

            for (int i = 0; i < min_max.size()-1; i = i+2)
            {
                sdfData *d;
                if (min_max.size() > 2)
                {
                    d = new sdfData("range_option_" + to_string(i/2), "",
                            json_type_undef);
                    data->addChoice(d);
                }
                else
                    d = data;

                if (min_max[i] == min_max[i+1])
                {
                    if (min_max[i] < -MAX_INT-1)
                        d->setConstantInt(-MAX_INT-1);
                    else if (min_max[i] > MAX_INT)
                        d->setConstantInt(MAX_INT);
                    else
                        d->setConstantInt(int(min_max[i]));
                }
                else
                {
                    if (min_max[i] >= -MAX_INT)
                        d->setMinimum((int)min_max[i]);
                        //min = (int)min_max[i];
                    if (min_max[i+1] < MAX_INT)
                        d->setMaximum((int)min_max[i+1]);
                        //max = (int)min_max[i+1];

                    //d->setIntData(-1, false, -1, false, min, max);
                    //d->setType(json_type_undef);
                }
            }
        }
    }

    return data;
}

/*
 * The information is extracted from the given lys_tpdf struct and
 * a corresponding sdfData object is generated
 */
sdfData* typedefToSdfData(struct lys_tpdf *tpdf)
{
    sdfData *data = new sdfData(avoidNull(tpdf->name), avoidNull(tpdf->dsc),
            parseTypeToString(&tpdf->type));

    // translate the status of the typedef
    data->setDescription(statusToDescription(tpdf->flags,
            data->getDescription()));

    // translate units
    string units = "";
    if (tpdf->units != NULL)
    {
        data->setUnits(tpdf->units);
    }

    typeToSdfData(&tpdf->type, data);
    data->parseDefault(tpdf->dflt);
    //parseDefault(tpdf->dflt, data);

    typedefs[tpdf->name] = data;
    return data;
}

/*
 * The information is extracted from the given lys_node_leaf struct and
 * a corresponding sdfProperty object is generated
 */
sdfProperty* leafToSdfProperty(struct lys_node_leaf *node,
        sdfObject *object = NULL)
{
    sdfProperty *property = new sdfProperty(avoidNull(node->name),
            avoidNull(node->dsc),
            parseBaseType(node->type.base));

    // translate the status of the leaf
    property->setDescription(statusToDescription(node->flags,
            property->getDescription()));

    property->setDescription(mustToDescription(node->must, node->must_size,
                property->getDescription()));

    //else
    //{
    typeToSdfData(&node->type, property);
    // save reference to leaf for ability to convert leafref-type
    if (node->type.base != LY_TYPE_LEAFREF) // TODO: keep this condition?
        leafs[generatePath((lys_node*)node)] = property;
    //}

    if (node->units != NULL)
        property->setUnits(node->units);

    if (node->nodetype != LYS_LEAFLIST)
    {
        property->parseDefault(node->dflt);
        property->setDescription(whenToDescription(node->when,
                property->getDescription()));
        property->setDescription(
                mustToDescription(
                    ((lys_node_leaf*)node)->must,
                    ((lys_node_leaf*)node)->must_size,
                    property->getDescription()));
    }
        //parseDefault(node->dflt, property);

    // TODO: keyword mandatory -> sdfRequired
    return property;
}

sdfData* leafToSdfData(struct lys_node_leaf *node,
        sdfObject *object = NULL)
{
    sdfData *data = new sdfData(*leafToSdfProperty(node, object));


    if (node->type.base == LY_TYPE_LEAFREF)
    {
        if (node->type.info.lref.target)
            referencesLeft.push_back(tuple<string, sdfCommon*>{
               generatePath((lys_node*)node->type.info.lref.target),
                       (sdfCommon*)data});
        else
            referencesLeft.push_back(tuple<string, sdfCommon*>{
               expandPath(node), (sdfCommon*)data});
        //cout<<"Leafref added "<<expandPath(node)<< endl;
    }
    // find what leafToSdfProperty returns in typerefs and overwrite with data
    else
    {
        if (stringToJsonDType(node->type.der->name) == json_type_undef
                && node->type.base != LY_TYPE_ENUM
                && node->type.base != LY_TYPE_IDENT
                && strcmp(node->type.der->name, "empty") != 0)
        {
            typerefs.push_back(tuple<string, sdfCommon*>{
                node->type.der->name, data});
            //cout <<"Typeref added "<< data->getName() << endl;
        }

        leafs[generatePath((lys_node*)node)] = data;
        //cout<<"Leaf added "<<generatePath((lys_node*)node)<<endl;
    }
    assert(!dynamic_cast<sdfProperty*>(data));
    return data;
}

/*
 * The information is extracted from the given lys_node_leaflist struct and
 * a corresponding sdfProperty object is generated
 */
sdfProperty* leaflistToSdfProperty(struct lys_node_leaflist *node,
        sdfObject *object = NULL)
{
    sdfProperty *property;
    try
    {
        property = new sdfProperty(avoidNull(node->name),
                "", json_array);
    }
    catch (int i)
    {
        cerr << "allocation failed" << endl;
        exit(EXIT_FAILURE);
    }

    property->setDescription(whenToDescription(node->when,
            property->getDescription()));
    property->setDescription(
            mustToDescription(
                ((lys_node_leaflist*)node)->must,
                ((lys_node_leaflist*)node)->must_size,
                property->getDescription()));

    //property->setType("array");
    sdfData *itemConstr = leafToSdfData((lys_node_leaf*)node);
    // item constraints do not have a unit or a label
    itemConstr->setName("");
    itemConstr->setUnits("");

    property->setItemConstr(itemConstr);

    // add reference to list
    /*if (node->type.base == LY_TYPE_LEAFREF)
        referencesLeft.push_back(tuple<string, sdfCommon*>{
            expandPath(node), (sdfCommon*)itemConstr});
    */
    // overwrite reference for path
    // TODO: is this necessary? also happens in leafToSdfData
    if (node->type.base != LY_TYPE_LEAFREF)
        leafs[generatePath((lys_node*)node)] = property;

    // if the node has type leafref
    /*if (node->type.base == LY_TYPE_LEAFREF)
    {
        //if (leafs[expandPath(node)])
        //    itemConstr->setReference(leafs[expandPath(node)]);
        //else
       referencesLeft.push_back(tuple<string, sdfCommon*>{
           expandPath(node), (sdfCommon*)itemConstr});

        cout << "   leaflist: " << node->name << " " << expandPath(node) << endl;
        property->setArrayData(NAN, NAN, false, itemConstr);
    }

    else
    {
        // convert all information from the type of the node
        //typeToSdfData(&node->type, property);
        typeToSdfData(&node->type, itemConstr);
        property->setItemConstr(itemConstr);

        // save reference to leaf-list for ability to convert leafref-type
        leafs[generatePath((lys_node*)node)] = property;
    }*/

    // the number of minimal and maximal items is only valid if at least
    // one of them is not 0
    if (node->min != 0)
        property->setMinItems(node->min);
    if (node->max != 0)
        property->setMaxItems(node->max);
    // translate units
    if (node->units != NULL)
        property->setUnits(node->units);

    property->parseDefaultArray(node);
    //parseDefaultArray(property, node);

    return property;
}

sdfData* leaflistToSdfData(struct lys_node_leaflist *node,
        sdfObject *object = NULL)
{
    sdfData *data = new sdfData(*leaflistToSdfProperty(node, object));

    if (node->type.base != LY_TYPE_LEAFREF)
       leafs[generatePath((lys_node*)node)] = data;

    assert(!dynamic_cast<sdfProperty*>(data));
    return data;
}

sdfData* listToSdfData(struct lys_node_list *node)
{
    sdfData *data = new sdfData(avoidNull(node->name),
            avoidNull(node->dsc), json_array);


    return data;
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
 * The information is extracted from the given lys_node_rpc_action struct and
 * a corresponding sdfAction object is generated
 * In case of an action node, data has to be given
 */
sdfAction* actionToSdfAction(lys_node_rpc_action *node, sdfObject *object,
        sdfData *data = NULL)
{
    sdfAction *action = new sdfAction(avoidNull(node->name),
            avoidNull(node->dsc));
    if (node->ref != NULL)
        action->setDescription(action->getDescription()
                + "\n\nReference:\n" + avoidNull(node->ref));

    // translate the status of the notification
    action->setDescription(statusToDescription(node->flags,
            action->getDescription()));

    for (int i = 0; i < node->tpdf_size; i++)
        action->addDatatype(typedefToSdfData(&node->tpdf[i]));

    struct lys_node *start = node->child;
    struct lys_node *elem;
    LY_TREE_FOR(start, elem)
    {
        // A YANG action/rpc has at most 1 input
        if (elem->nodetype == LYS_INPUT && elem->flags != LYS_IMPLICIT)
        {
            //sdfData *input = new sdfData("", "", "object");
            sdfData *inputProps = nodeToSdfData(elem, object);
            if (data)
            {
                sdfData *input = new sdfData("", "", "object");
                // TODO: will a copy made like this cause problems?
                sdfData *inputPropsInlay = new sdfData(*data);
                inputProps->setName(action->getName());
                // TODO: what if child node of input is an 'only child'?
                inputPropsInlay->addObjectProperty(inputProps);
                // What if data is of type array? translation of list?
                // -> not a problem because data is still the itemConstr of
                // the list->array at this point which is of type object
                input->addObjectProperty(inputPropsInlay);
                // TODO: is the eliciting node->data required in the input?
                input->addRequiredObjectProperty(inputPropsInlay->getName());

                input->setDescription(
                        mustToDescription(
                            ((lys_node_inout*)node)->must,
                            ((lys_node_inout*)node)->must_size,
                            input->getDescription()));

                action->setInputData(input);
            }
            // TODO: set name of inputProps to "" ?
            else
            {
                inputProps->setDescription(
                        mustToDescription(
                            ((lys_node_inout*)node)->must,
                            ((lys_node_inout*)node)->must_size,
                            inputProps->getDescription()));
                action->setInputData(inputProps);
            }

            //action->setInputData(input);
        }
        // A YANG action/rpc has at most 1 output
        else if (elem->nodetype == LYS_OUTPUT && elem->flags != LYS_IMPLICIT)
        {
            sdfData* output = nodeToSdfData(elem, object);
            output->setName("");
            output->setDescription(
                    mustToDescription(
                        ((lys_node_inout*)node)->must,
                        ((lys_node_inout*)node)->must_size,
                        output->getDescription()));
            /*
             * TODO: if the child node is an 'only child' should not it be
             * translated to object properties of the output but to output
             * directly?
             */
            //if (elem->child->next)
            // output = nodeToSdfData(elem, object);
            //else
            //output = nodeToSdfData(elem, object)->getObjectProperties()[0];
            action->setOutputData(output);
        }
    }

    // TODO: if-feature

    return action;
}

sdfData* nodeToSdfData(struct lys_node *node, sdfObject *object)
{
    sdfData *data;
    try
    {
        data = new sdfData(avoidNull(node->name),
                avoidNull(node->dsc), json_object);
    }
    catch(int i)
    {
        cerr << "allocation failed" << endl;
        exit(EXIT_FAILURE);
    }
    if (node->ref != NULL)
        data->setDescription(data->getDescription()
                + "\n\nReference:\n" + avoidNull(node->ref));

    // translate the status of the node
    data->setDescription(statusToDescription(node->flags,
            data->getDescription()));

    // TODO: if feature
    for (int i = 0; i < node->iffeature_size; i++);

    // translate the when-statement (if applicable)
    if (node->nodetype == LYS_CONTAINER || node->nodetype == LYS_CHOICE
            || node->nodetype == LYS_ANYDATA || node->nodetype == LYS_CASE )
    {
        data->setDescription(
                whenToDescription(((lys_node_container*)node)->when,
                        data->getDescription()));
    }

    // iterate over all child nodes
    struct lys_node *start = node->child;
    struct lys_node *elem;
    LY_TREE_FOR(start, elem)
    {
        //TODO: flags

        /*
         * Containers are translated to sdfData, which cannot have
         * sdfActions. Therefore, YANG actions that are children of
         * containers are translated to sdfActions of the sdfObject
         * that the container-sdfData belongs to. The container-sdfData
         * is referenced as part of the input for that sdfAction.
         */
        // Actions, unlike RPCs, can be tied to containers or lists
        if (elem->nodetype == LYS_ACTION)
        {
            sdfAction *action = actionToSdfAction(
                    (lys_node_rpc_action*)elem, object, data);
            action->setDescription(
                    "Action connected to " + avoidNull(node->name) + "\n\n"
                    + action->getDescription());
            object->addAction(action);
        }
        if (elem->nodetype == LYS_CASE)
        {
            sdfData *c = nodeToSdfData(elem, object);
            data->addChoice(c);
            branchRefs[generatePath(elem)] = c;

            c->setDescription(whenToDescription(
                    ((lys_node_case*)elem)->when, c->getDescription()));
        }
        if (elem->nodetype == LYS_CHOICE)
        {
            sdfData *choiceData = nodeToSdfData(elem, object);

            for (sdfData *prop : choiceData->getObjectProperties())
                choiceData->addChoice(prop);
            choiceData->setObjectProperties({});

            data->addObjectProperty(choiceData);
            if (elem->flags && LYS_MAND_MASK == LYS_MAND_TRUE)
                object->addRequired((sdfCommon*)choiceData);

            // set the reference to the sdfData element of the
            // corresponding default case as default
            if (((lys_node_choice*)elem)->dflt)
            {
                sdfData *dflt = new sdfData(
                        ((lys_node_choice*)elem)->dflt->name, "", "");
                dflt->setReference(
                    branchRefs[generatePath(((lys_node_choice*)elem)->dflt)]);
                choiceData->setDefaultObject(dflt);
            }
            choiceData->setDescription(whenToDescription(
                 ((lys_node_choice*)elem)->when, choiceData->getDescription()));
        }
        else if (elem->nodetype == LYS_CONTAINER)
        {
            for (int i = 0; i < ((lys_node_container*)elem)->tpdf_size; i++)
                object->addDatatype(typedefToSdfData(
                        &((lys_node_container*)elem)->tpdf[i]));

            sdfData *container = nodeToSdfData(elem, object);
            container->setDescription(
                    mustToDescription(
                        ((lys_node_container*)elem)->must,
                        ((lys_node_container*)elem)->must_size,
                        container->getDescription()));
            data->addObjectProperty(container);

            // TODO: presence

            if (((lys_node_container*)elem)->presence);
        }
        else if (elem->nodetype == LYS_GROUPING)
        {
            sdfData *data = nodeToSdfData(elem, object);
            object->addDatatype(data);
            branchRefs[generatePath(elem)] = data;
        }
        else if (elem->nodetype == LYS_LEAF)
        {
            sdfData *next = leafToSdfData((lys_node_leaf*)elem, object);

            if ((elem->flags & LYS_UNIQUE) == LYS_UNIQUE);
                // TODO: now what?

            data->addObjectProperty(next);
            if ((elem->flags & LYS_MAND_MASK) == LYS_MAND_TRUE)
                data->addRequiredObjectProperty(avoidNull(elem->name));

            //leafs[generatePath(elem)] = next;
        }
        else if (elem->nodetype == LYS_LEAFLIST)
        {
            sdfData *next = leaflistToSdfData((lys_node_leaflist*)elem, object);
            data->addObjectProperty(next);
            //leafs[generatePath(elem)] = next;

            for (int i = 0; i < ((lys_node_leaflist*)elem)->must_size; i++);
        }
        else if (elem->nodetype == LYS_LIST)
        {
            sdfData *next = new sdfData(avoidNull(elem->name),
                    avoidNull(elem->dsc), json_array);

            // translate the status of the list node
            next->setDescription(statusToDescription(elem->flags,
                    next->getDescription()));
            next->setDescription(whenToDescription(
                    ((lys_node_list*)elem)->when, next->getDescription()));
            next->setDescription(
                    mustToDescription(
                        ((lys_node_list*)elem)->must,
                        ((lys_node_list*)elem)->must_size,
                        next->getDescription()));

            sdfData *itemConstr = nodeToSdfData(elem, object);
            itemConstr->setName("");
            next->setItemConstr(itemConstr);

            for (int i = 0; i < ((lys_node_list*)elem)->tpdf_size; i++)
                object->addDatatype(typedefToSdfData(
                        &((lys_node_list*)elem)->tpdf[i]));

            if (((lys_node_list*)elem)->min != 0)
                next->setMinItems(((lys_node_list*)elem)->min);
            if (((lys_node_list*)elem)->max != 0)
                next->setMaxItems(((lys_node_list*)elem)->max);

            data->addObjectProperty(next);

            // TODO: unique, keys
        }
        else if (elem->nodetype == LYS_NOTIF)
        {
            sdfEvent *event = new sdfEvent(avoidNull(elem->name),
                    avoidNull(elem->dsc));

            // translate the status of the notification
            event->setDescription(statusToDescription(elem->flags,
                    event->getDescription()));
            // transfer the must statement to the description
            event->setDescription(
                    mustToDescription(
                        ((lys_node_notif*)elem)->must,
                        ((lys_node_notif*)elem)->must_size,
                        event->getDescription()));

            sdfData *output = nodeToSdfData(elem, object);
            output->setName("");
            output->setDescription("");
            event->setOutputData(output);
            event->setDescription(
                    "Notification from " + avoidNull(node->name) + ":\n\n"
                    + event->getDescription());
            object->addEvent(event);

            for (int i = 0; i < ((lys_node_notif*)elem)->tpdf_size; i++)
                object->addDatatype(typedefToSdfData(
                        &((lys_node_notif*)elem)->tpdf[i]));
        }
        else if (elem->nodetype == LYS_USES)
        {
            sdfData *uses;
            // if there are alterations to the referenced grouping
            // it cannot be referenced anymore but has to be inserted fully
            // in the altered version
            if (((lys_node_uses*)elem)->augment_size > 0
                    || ((lys_node_uses*)elem)->refine_size > 0)
                uses = nodeToSdfData(elem, object);

            else
            {
                uses = new sdfData(avoidNull(elem->name),
                    avoidNull(elem->dsc), "", branchRefs[generatePath(
                    (lys_node*)((lys_node_uses*)elem)->grp)]);
                // translate the status of the uses node
                uses->setDescription(statusToDescription(elem->flags,
                        uses->getDescription()));
            }
            uses->setDescription(whenToDescription(
                    ((lys_node_uses*)elem)->when, uses->getDescription()));

            data->addObjectProperty(uses);
        }
        // TODO: presence?, anydata, anyxml
    }
    // TODO: really add tpdfs to object or rather to last parent property etc?
    return data;
}

sdfData* identToSdfData(struct lys_ident _ident)
{
    sdfData *ident = new sdfData(avoidNull(_ident.name),
            avoidNull(_ident.dsc), "");

    // translate the status of the identity
    ident->setDescription(statusToDescription(_ident.flags,
            ident->getDescription()));

    if (_ident.ref != NULL)
        ident->setDescription(ident->getDescription()
                + "\n\nReference:\n" + _ident.ref);
    // TODO: if-feature
    if (_ident.base_size > 1)
    {
        sdfData *ref;
        for (int j = 0; j < _ident.base_size; j++)
        {
            ident->setType("object");
            ref = new sdfData();
            ref->setName("base_" + j);
            //ref->setReference(identities[_ident.base[j]->name]);
            if (identities[_ident.base[j]->name])
                ref->setReference(identities[_ident.base[j]->name]);
            else
            {
                cerr << "identToSdfData: identity reference is null" << endl;
                identsLeft.push_back(tuple<string, sdfCommon*>{
                    _ident.base[j]->name, (sdfCommon*)ref});
            }
            ident->addObjectProperty(ref);
        }
    }
    else if (_ident.base_size == 1)
    {
        if (identities[_ident.base[0]->name])
            ident->setReference(identities[_ident.base[0]->name]);
        else
        {
            cerr << "typeToSdfData: identity reference is null" << endl;
            identsLeft.push_back(tuple<string, sdfCommon*>{
                _ident.base[0]->name, (sdfCommon*)ident});
        }
    }

    return ident;
}

vector<tuple<string, sdfCommon*>> assignReferences(
        vector<tuple<string, sdfCommon*>> refsLeft,
        map<string, sdfCommon*> refs)
{
    // check for references left unassigned
    string str;
    sdfCommon *com;
    vector<tuple<string, sdfCommon*>> stillLeft = {};

    for (tuple<string, sdfCommon*> r : refsLeft)
    {
        tie(str, com) = r;
        if (com && refs[str])
            com->setReference(refs[str]);
        else
            stillLeft.push_back(r);
    }

    return stillLeft;
}

/*
 * The information is extracted from the given lys_module struct and
 * a corresponding sdfThing object is generated
 */
sdfObject* moduleToSdfObject(const struct lys_module *module)
{
    sdfObject *object = new sdfObject(avoidNull(module->name),
            avoidNull(module->dsc));

    if (module->ref && module->rev_size == 0)
        object->setDescription(object->getDescription()
                + "\n\nReference:\n" + avoidNull(module->ref));

    else if (module->rev_size > 0) // TODO: ignore the older revisions?
        object->setDescription(object->getDescription()
                + "\n\nReference:\n" + avoidNull(module->rev[0].ref));

    if (module->org != NULL)
        object->setDescription(object->getDescription()
                + "\n\nOrganization:\n" + avoidNull(module->org));

    if (module->contact != NULL)
        object->setDescription(object->getDescription()
                + "\n\nContact:\n" + avoidNull(module->contact));

    // The description of the module is searched for the terms 'license' and
    // 'copyright' to transfer matching paragraphs
    string copyright;
    string dsc = avoidNull(module->dsc);
    // find a paragraph that starts with 'Copyright'
    regex copyrightRegex("[Cc]opyright.*(\n.+)*");
    smatch sm;
    regex_search(dsc, sm, copyrightRegex);
    copyright = sm[0];
    string license;
    // find a paragraph that contains the word 'license'
    regex licenseRegex("(.+\n)*.*[Ll]icense.*(\n.+)*");
    regex_search(dsc, sm, licenseRegex);
    license = sm[0];
    object->setInfo(new sdfInfoBlock(avoidNull(module->name),
            avoidNull(module->rev->date), copyright, license));

    // Convert the namespace information
    map<string, string> nsMap;
    nsMap[avoidNull(module->prefix)] = avoidNull(module->ns);
    object->setNamespace(new sdfNamespaceSection(nsMap));

    // TODO: feature, deviation, extension, import, anydata?, anyxml?

    // Translate typedefs of the module to sdfData of the sdfObject
    for (int i = 0; i < module->tpdf_size; i++)
        object->addDatatype(typedefToSdfData(&module->tpdf[i]));


    // Translate identities of the module to sdfData of the sdfObject
    sdfData *ident;
    for (int i = 0; i < module->ident_size; i++)
    {
        ident = identToSdfData(module->ident[i]);
        object->addDatatype(ident);
        identities[module->ident[i].name] = ident;
    }

    // Add identities and typedefs of the submodule to the sdfObject
    // TODO: what about the other members of submodule and further levels of
    // submodules?
    for (int i = 0; i < module->inc_size; i++)
    {
        for (int j = 0; j < module->inc[i].submodule->tpdf_size; j++)
            object->addDatatype(typedefToSdfData(
                    &module->inc[i].submodule->tpdf[j]));

        sdfData *ident;
        for (int j = 0; j < module->inc[i].submodule->ident_size; j++)
        {
            ident = identToSdfData(module->inc[i].submodule->ident[j]);
            object->addDatatype(ident);
            identities[module->inc[i].submodule->ident[j].name] = ident;
        }
    }

    // Translate imported modules
    for (int i = 0; i < module->imp_size; i++)
    {
        sdfObject *importObject = moduleToSdfObject(module->imp[i].module);
        importObject->objectToFile(avoidNull(module->imp[i].module->name)
                + ".sdf.json");
    }

    // if the module is actually a submodule, return without iterating over
    // the child nodes (which do not exist in a submodule with type 1)
    //if (module->type == 1)
      //  return object;

    // iterate over all child nodes of the module
    struct lys_node *start = module->data;
    struct lys_node *elem;
    LY_TREE_FOR(start, elem)
    {
        // check node type (container, grouping, leaf, etc.)
        if (elem->nodetype == LYS_CHOICE)
        {
            // TODO: test
            sdfProperty *property = new sdfProperty(avoidNull(elem->name),
                    avoidNull(elem->dsc));
            // translate the status of the choice node
            property->setDescription(statusToDescription(elem->flags,
                    property->getDescription()));

            sdfData *choiceData = nodeToSdfData(elem, object);

            for (sdfData *prop : choiceData->getObjectProperties())
                choiceData->addChoice(prop);
            choiceData->setObjectProperties({});

            property->addObjectProperty(choiceData);
            // set the reference to the sdfData element of the
            // corresponding default case as default
            if (((lys_node_choice*)elem)->dflt)
            {
                sdfData *dflt = new sdfData(
                        ((lys_node_choice*)elem)->dflt->name, "", "");
                dflt->setReference(
                    branchRefs[generatePath(((lys_node_choice*)elem)->dflt)]);
                choiceData->setDefaultObject(dflt);
            }

            object->addProperty(property);
            if (elem->flags && LYS_MAND_MASK == LYS_MAND_TRUE)
                object->addRequired((sdfCommon*)choiceData);

            //TODO: default choice (sdfRef?), when, if-feature?
            if (((lys_node_container*)elem)->when);
        }
        else if (elem->nodetype == LYS_CONTAINER)
        {
            sdfProperty *property = new sdfProperty(
                    *nodeToSdfData(elem, object));

            for (int i = 0; i < ((lys_node_container*)elem)->tpdf_size; i++)
                object->addDatatype(typedefToSdfData(
                        &((lys_node_container*)elem)->tpdf[i]));

            // TODO: typedef, when, must, presence
            for (int i = 0; i < ((lys_node_container*)elem)->must_size; i++);
            if (((lys_node_container*)elem)->when);
            if (((lys_node_container*)elem)->presence);

            object->addProperty(property);
        }
        else if (elem->nodetype == LYS_GROUPING)
        {
            // Groupings are not converted?
            // They are transfered when referenced by 'uses'

            sdfData *data = nodeToSdfData(elem, object);
            object->addDatatype(data);
            branchRefs[generatePath(elem)] = data;
        }
        else if (elem->nodetype == LYS_LEAF)
        {
            // leafs are converted to sdfProperties at the top level
            sdfProperty *next = leafToSdfProperty((lys_node_leaf*)elem, object);
            object->addProperty(next);
            if ((elem->flags & LYS_MAND_MASK) == LYS_MAND_TRUE)
                object->addRequired(next);

            if ((elem->flags & LYS_UNIQUE) == LYS_UNIQUE);
                // TODO: now what?
        }
        else if (elem->nodetype == LYS_LEAFLIST)
        {
            // leaf-lists are converted to sdfProperty at the top level
            object->addProperty(leaflistToSdfProperty(
                    (lys_node_leaflist*)elem));

            // TODO: must, when
            if (((lys_node_leaflist*)elem)->when);
            for (int i = 0; i < ((lys_node_leaflist*)elem)->must_size; i++);
        }
        else if (elem->nodetype == LYS_LIST)
        {
            // lists are converted to sdfProperty at the top level
            sdfProperty *next = new sdfProperty(
                    avoidNull(elem->name), avoidNull(elem->dsc), json_array);

            // translate the status of the list node
            next->setDescription(statusToDescription(elem->flags,
                    next->getDescription()));

            sdfData *itemConstr = nodeToSdfData(elem, object);
            itemConstr->setName("");
            next->setItemConstr(itemConstr);

            // TODO: really add to object or rather to last parent property etc
            for (int i = 0; i < ((lys_node_list*)elem)->tpdf_size; i++)
                object->addDatatype(typedefToSdfData(
                        &((lys_node_list*)elem)->tpdf[i]));

            if (((lys_node_list*)elem)->min != 0)
                next->setMinItems(((lys_node_list*)elem)->min);
            if (((lys_node_list*)elem)->max != 0)
                next->setMaxItems(((lys_node_list*)elem)->max);

            object->addProperty(next);

            // TODO: unique, keys, must, when
        }
        else if (elem->nodetype == LYS_NOTIF)
        {
            sdfEvent *event = new sdfEvent(avoidNull(elem->name),
                    avoidNull(elem->dsc));

            // translate the status of the notification
            event->setDescription(statusToDescription(elem->flags,
                    event->getDescription()));

            sdfData *output = nodeToSdfData(elem, object);
            output->setName("");
            output->setDescription("");
            event->setOutputData(output);
            object->addEvent(event);

            for (int i = 0; i < ((lys_node_notif*)elem)->tpdf_size; i++)
                object->addDatatype(typedefToSdfData(
                        &((lys_node_notif*)elem)->tpdf[i]));

            //TODO: must, iffeature
        }
        // YANG modules cannot have actions at the top level, only rpcs
        else if (elem->nodetype == LYS_RPC)
        {
            // rpcs are converted to sdfAction
            object->addAction(actionToSdfAction(
                    (lys_node_rpc_action*)elem, object));
        }
        else if (elem->nodetype == LYS_USES)
        {
            sdfProperty *uses;
            // if there are alterations to the referenced grouping
            // it cannot be referenced anymore but has to be inserted fully
            // in the altered version
            if (((lys_node_uses*)elem)->augment_size > 0
                    || ((lys_node_uses*)elem)->refine_size > 0)
                uses = new sdfProperty(*nodeToSdfData(elem, object));

            else
            {
                uses = new sdfProperty(avoidNull(elem->name),
                    avoidNull(elem->dsc), json_type_undef,
                    branchRefs[generatePath(
                            (lys_node*)((lys_node_uses*)elem)->grp)]);
                // translate the status of the uses node
                uses->setDescription(statusToDescription(elem->flags,
                        uses->getDescription()));
            }

            if (((lys_node_container*)elem)->when);
                // TODO

            object->addProperty(uses);
        }
    }

    // check for references left unassigned
    referencesLeft = assignReferences(referencesLeft, leafs);
    typerefs = assignReferences(typerefs, typedefs);
    identsLeft = assignReferences(identsLeft, identities);

    if (!referencesLeft.empty() || !typerefs.empty() || !identsLeft.empty())
        cerr << object->getName() << ": Unresolved references remaining"
        << endl;


    /*string str;
    sdfCommon *com;
    cout << "leafrefs: " << endl;
    for (tuple<string, sdfCommon*> t : referencesLeft)
    {
        tie(str, com) = t;
        cout << str << endl;
    }
    cout << "typerefs: " << endl;
    for (tuple<string, sdfCommon*> t : typerefs)
    {
        tie(str, com) = t;
        cout << str << endl;
    }*/

    return object;
}

/*
 * The information is extracted from the given lys_module and
 * a corresponding sdfThing object is generated
 * If there are no submodules in the module, it has to be translated to
 * sdfObject instead of sdfThing
 */
/*
sdfThing* moduleToSdfThing(const struct lys_module *module)
{
    // if there are no submodules in the module, it has to be translated to
    // sdfObject instead of sdfThing
    if (module->inc_size == 0)
        return NULL;

    // TODO: choose name and dsc of thing
    sdfThing *thing = new sdfThing(avoidNull(module->name),
            avoidNull(module->dsc));

    // the content of the module is translated to sdfObject
    thing->addObject(moduleToSdfObject(module));

    for (int i = 0; i < module->inc_size; i++)
    {
        // if the submodule does not include further submodules
        // it gets translated to sdfObject
        if (module->inc[i].submodule->inc_size == 0)
            thing->addObject(moduleToSdfObject(
                    (lys_module*)module->inc[i].submodule));
        // else it is translated to sdfThing
        else if (module->inc[i].submodule->inc_size > 0)
            thing->addThing(moduleToSdfThing(
                    (lys_module*)module->inc[i].submodule));
    }

    return thing;
}

 * This function can be called to translate a submodule as part of its
 * 'super' module
 * If only the submodule should be translated, moduleToSdfThing() or
 * moduleToSdfObject() can be used
 *//*
sdfThing* submoduleToSdfThing(struct lys_submodule *submodule)
{
    sdfThing *thing = new sdfThing(avoidNull(submodule->belongsto->name),
            avoidNull(submodule->belongsto->dsc));

    thing->addObject(moduleToSdfObject(submodule->belongsto));

    if (submodule->inc_size == 0)
        thing->addObject(moduleToSdfObject((lys_module*)submodule));
    else
        thing->addThing(moduleToSdfThing((lys_module*)submodule));

    return thing;
}*/

void fillLysType(sdfData *data, struct lys_type *type)
{
    type->base = stringToLType(data->getSimpType());
    if (!data->getEnumString().empty())
    {
        type->base = LY_TYPE_ENUM;
        type->der = &enumTpdf;
        //int enmSize = 0; // TODO: clean this up
        //static std::vector<std::string> enmNames;
        /*
        if (!data->getEnumString().empty())
        {
            std::vector<std::string> strings = data->getEnumString();
            enmSize = strings.size();
            enmNames.resize(enmSize);
            for (int i = 0; i < enmSize; i++)
            {
                enmNames[i] = strings[i];
            }
        }*/
        int enmSize = data->getEnumString().size();
        vector<string> enm = data->getEnumString();
        type->info.enums.count = enmSize;
        //static std::vector<lys_type_enum> enms(enmSize);
        for (int i = 0; i < enmSize; i++)
        {
            //type->info.enums.enm[i] = new lys_type_enum();
            type->info.enums.enm[i].name = enm[i].c_str();
            type->info.enums.enm[i].value = i;
            //enms[i].name = enm[i].c_str();
            //enms[i].value = i;
        }
        //type->info.enums.enm = enms.data();
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
    else if (data->getReference())
    {
        sdfData *ref = dynamic_cast<sdfData*>(data->getReference());
        // ifref is of type int, number, string, bool or array with items
        // of said types
        if (ref && ref->getSimpType() != json_object
                && (ref->getSimpType() != json_array
                        || ref->getItemConstr()->getSimpType() != json_object))
        {
            type->der = &leafrefTpdf;
            type->info.lref.path = "test"; //TODO
        }
        // if ref is of type object or array with items of type object
        else if (ref && (ref->getSimpType() == json_array
                || ref->getSimpType() == json_object))
        {
            sdfProperty *propRef = dynamic_cast<sdfProperty*>
                (data->getReference());
            // if data references an sdfProperty
            if (propRef)
            {
                // TODO: uses of grouping that has to be created
                ((lys_node*)type->parent)->nodetype = LYS_USES;
            }
            else
            {
                // TODO: uses of existing grouping
                ((lys_node*)type->parent)->nodetype = LYS_USES;
            }
        }
        // if data references an sdfObject
        else
        {
            // TODO
        }
    }
}
/*
void addNodeToTree(lys_node *node, lys_node *prev, lys_node *parent,
        lys_module *module, bool first, bool last)
{
    // add the current node into the tree
    if (node)
    {
        node->module = module;
        node->parent = (lys_node*)&parent;
        if (first)
        {
            node->prev = (lys_node*)node;
            parent->child = (lys_node*)node;
        }
        else
        {
            if (!last)
                prev->next = (lys_node*)node;
            node->prev = (lys_node*)prev;
        }
        prev = (lys_node*)node;
    }
}*/

struct lys_tpdf * sdfDataToTypedef(sdfData *data, struct lys_tpdf *tpdf)
{
    tpdf->name = data->getNameAsArray();
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
/*
void sdfDataToList(sdfData *prop, lys_node_list *node)
{
    // for type array with items of type object?
    if (prop->getSimpType() != json_array ||
            (prop->getItemConstr()
                    && prop->getItemConstr()->getSimpType() != json_object))
        return;
}

lys_node_container* sdfDataToContainer(sdfData *data, lys_node_container *n)
{
    // for type object
    if (data->getSimpType() != json_object)
        return NULL;

    lys_node_container *cont = new lys_node_container();
    for (sdfData *prop : data->getObjectProperties())
    {
        lys_node_leaf *leaf = sdfDataToLeaf(prop);

        addNodeToTree()
    }
    return cont;
}

void sdfDataToLeaflist(sdfData *prop, lys_node_leaflist *leaflist)
{
    if (prop->getSimpType() != json_array)
        return;

    leaflist->name = prop->getNameAsArray();
    leaflist->dsc = prop->getDescriptionAsArray();
    if (prop->getUnits() != "")
        leaflist->units = prop->getUnitsAsArray(); // optional
    leaflist->nodetype = LYS_LEAFLIST;

    leaflist->dflt = (const char**)prop->getDefaultAsCharArray();
    unsigned int min = 1;
    //if (!isnan(prop->getMinItems()))
    leaflist->min = prop->getMinItems();
    //if (!isnan(prop->getMaxItems()))
    leaflist->max = prop->getMaxItems();

    fillLysType(prop->getItemConstr(), &leaflist->type);

}

void sdfDataToLeaf(sdfData *prop, lys_node_leaf *leaf)
{
    if (prop->getSimpType() == json_array || prop->getSimpType() == json_object)
        return;

    leaf->name = prop->getNameAsArray();
    leaf->dsc = prop->getDescriptionAsArray();
    if (prop->getUnits() != "")
        leaf->units = prop->getUnitsAsArray(); // optional

    leaf->dflt = prop->getDefaultAsCharArray();
    leaf->nodetype = LYS_LEAF;
    fillLysType(prop, &leaf->type);
    leaf->type.parent = (lys_tpdf*)leaf;
}*/

void addNode(lys_node *child, lys_node *parent, lys_module *module)
{
    if (!parent || !child || !module)
    {
        cerr << "addNode: child or parent node or module must not be null"
                << endl;
        return;
    }
    child->parent = parent;
    child->module = module;

    if (!parent->child)
    {
        parent->child = child;
        child->prev = child;
    }
    else
    {
        lys_node *sibling = parent->child;
        while (sibling->next)
            sibling = sibling->next;
        sibling->next = child;
        child->prev = sibling;
    }
}

lys_node* sdfDataToNode(sdfData *data, lys_node *node, lys_module *module)
{
    //lys_node *node;
    //cout << data->getType() << endl;
    // type object
    if (data->getSimpType() == json_object)
    {
        lys_node_container *cont = new lys_node_container();
        cont->nodetype = LYS_CONTAINER;
        vector<sdfData*> properties = data->getObjectProperties();
        for (int i = 0; i < properties.size(); i++)
        {
            lys_node *prev;
            lys_node *childNode;

            sdfDataToNode(properties[i], childNode, module);

            // add the current node into the tree
            if (childNode)
            {
                childNode->module = module;
                childNode->parent = node;
                if (i == 0)
                {
                    childNode->prev = childNode;
                    node->child = childNode;
                }
                else
                {
                    if (i < properties.size()-1)
                        prev->next = childNode;
                    childNode->prev = prev;
                }
                prev = childNode;
            }
        }
        node = (lys_node*)cont;
    }

    // type array with objects of type object
    else if (data->getSimpType() == json_array
            && data->getItemConstr()
               && data->getItemConstr()->getSimpType() == json_object)
    {
        lys_node_list *list = new lys_node_list();
        sdfDataToNode(data->getItemConstr(), (lys_node*)list, module);
        list->nodetype = LYS_LIST;
        node = (lys_node*)list;
    }

    // type array with objects of type int, number, string or bool
    else if (data->getSimpType() == json_array
            && (!data->getItemConstr()
               || data->getItemConstr()->getSimpType() != json_object))
    {
        lys_node_leaflist *leaflist = new lys_node_leaflist();
        leaflist->nodetype = LYS_LEAFLIST;

        fillLysType(data->getItemConstr(), &leaflist->type);
        leaflist->type.parent = (lys_tpdf*)leaflist;
        if (data->getUnits() != "")
            leaflist->units = data->getUnitsAsArray(); // optional
        leaflist->dflt = (const char**)data->getDefaultAsCharArray();
        //if (!isnan(prop->getMinItems()))
        leaflist->min = data->getMinItems();
        //if (!isnan(prop->getMaxItems()))
        leaflist->max = data->getMaxItems();

        node = (lys_node*)leaflist;
    }

    // type int, number, string or bool
    else if (data->getSimpType() != json_object
            && data->getSimpType() != json_array)
    {
        lys_node_leaf *leaf = new lys_node_leaf();
        leaf->nodetype = LYS_LEAF;

        fillLysType(data, &leaf->type); // TODO: const?
        leaf->type.parent = (lys_tpdf*)leaf;
        if (data->getUnits() != "")
            leaf->units = data->getUnitsAsArray();
        leaf->dflt = data->getDefaultAsCharArray();

        node = (lys_node*)leaf;
    }

    else
        cerr << "This should not happen" << endl;

    lys_node_choice *choice = new lys_node_choice();
    for (sdfData *c : data->getChoice())
    {
        lys_node_case *cas = new lys_node_case();
        lys_node *n = NULL;
        n = sdfDataToNode(c, (lys_node*)n, module);
        cas->nodetype = LYS_CASE;
        cas->child = n;
        node = (lys_node*)choice; //?
    }

    node->name = data->getNameAsArray();
    node->dsc = data->getDescriptionAsArray();
    return node;
}


struct lys_module* sdfObjectToModule(sdfObject &object, lys_module &module)
{
    /*const lys_module *buf_module = lys_parse_path(ly_ctx_new("./yang", 0),
                 "yang/standard/ietf/RFC/ietf-l2vpn-svc.yang", LYS_IN_YANG);

    struct lys_module *module = const_cast<lys_module*>(buf_module);*/
    //module->version = 0;

    module.type = 0;
    module.deviated = 0;
    module.imp_size = 0;
    module.inc_size = 0;
    module.ident_size = 0;
    module.features_size = 0;
    module.augment_size = 0;
    module.deviation_size = 0;
    module.extensions_size = 0;
    module.ext_size = 0;

    // title has to be altered
    string title = object.getInfo()->getTitle();
    // All special characters (like '(') have to be removed from the title
    title.resize(remove_if(title.begin(), title.end(),
            [](char x){return !isalnum(x) && !isspace(x);})-title.begin());
    // Spaces are replaced by '-'
    replace(title.begin(), title.end(), ' ', '-');
    // All characters in the title have to be lower case
    for (int i = 0; i < title.length(); i++)
        title[i] = tolower(title[i]);
    string *titleP = new string(title);
    module.name = titleP->c_str();

    // copyright and license of SDF are put into the module's description
    string *dsc = new string(object.getInfo()->getTitle()
            + "\n\n" + object.getDescription()
            + "\n\nCopyright: " + object.getInfo()->getCopyright()
            + "\nLicense: " + object.getInfo()->getLicense());
    /*
    const char *cat(dsc.c_str());
    std::strcpy(cat, object.getInfo()->getTitleAsArray());
    std::strcat(cat, "\n\n");
    std::strcat(cat, object.getDescriptionAsArray());
    std::strcat(cat, "\n\nCopyright: ");
    std::strcat(cat, object.getInfo()->getCopyrightAsArray());
    std::strcat(cat, "\nLicense: ");
    std::strcat(cat, object.getInfo()->getLicenseAsArray());
    //std::strcat(cat, "\nVersion: ");
    //std::strcat(cat, object.getInfo()->getVersionAsArray());
    */
    module.dsc = dsc->c_str();

    //module.dsc = dsc.c_str();
    module.prefix = object.getNamespace()->getNamespacesAsArrays().begin()
                                                                      ->first;
    module.ns = object.getNamespace()->getNamespacesAsArrays().begin()
                                                                      ->second;
    module.org = object.getInfo()->getCopyrightAsArray();
    module.ref = "";
    module.contact = "";
    string version = object.getInfo()->getVersion();
    module.rev = new lys_revision();
    memcpy(module.rev[0].date, object.getInfo()->getVersionAsArray(), 10);
    module.rev_size = 1;

    uint16_t cnt = 0;
    module.tpdf = new lys_tpdf[object.getDatatypes().size()]();
    if (module.tpdf == nullptr)
        cerr << "Error: memory could not be allocated" << endl;
    for (sdfData *i : object.getDatatypes())
    {
        sdfDataToTypedef(i, &module.tpdf[cnt]);
        module.tpdf[cnt].module = &module;

        cnt++;
    }
    module.tpdf_size = cnt;

    static struct lys_node_container props = {
            .name = "properties",
            .dsc = "",
            //.ref = NULL,
            //.flags = 0,
            //.ext_size = 0,
            //.iffeature_size = 0,
            //.must_size = 0,
            //.tpdf_size = 0,
            .module = &module,
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
    module.data = (lys_node*)&props;

    vector<sdfProperty*> properties = object.getProperties();

    //static vector<lys_node*> nodes(properties.size());
    lys_node *prev;
    lys_node *currentNode;
    for (sdfProperty *i : object.getProperties())
    {
        currentNode = NULL;//new lys_node();
        currentNode = sdfDataToNode(i, currentNode, &module);
        // add the current node into the tree
        addNode(currentNode, (lys_node*)&props, &module);


    }

    for (sdfAction *i : object.getActions())
    {

    }

    for (sdfEvent *i : object.getEvents())
    {

    }

    return &module;
}

/*
 * The information from a sdfThing is transferred into a YANG module
 */
struct lys_module* sdfThingToModule(sdfThing *thing)
{
    struct lys_module *module;
    printf("in sdfThingToModule\n");
    module->version = 0;
    module->name = thing->getName().c_str();
    //std::cout << thing->getName().c_str() << std::endl;
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
    if (argc < 3)
    {
        cerr << "missing input" << endl; // TODO
        return -1;
    }
    // regexs to check file formats
    std::regex yang_regex (".*\\.yang");
    std::regex sdf_json_regex (".*\\.sdf\\.json");

    // check whether input file is a YANG file
    if (std::regex_match(argv[1], yang_regex))
    {
        // TODO: check whether output file has right format
        if (argc > 3 && !std::regex_match(argv[2], sdf_json_regex))
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
        cout << "Module parsing succeeded" << endl;

        // Convert the module to a sdfThing
        //sdfThing *moduleThing = moduleToSdfThing(module);
        // Print the sdfThing into a file (in sdf.json format)
        //moduleThing->thingToFile(argv[2]);

        /*sdfThing *moduleThing = moduleToSdfThing(module);
        if (moduleThing)
        {
            cout << "in0" << endl;
            moduleThing->thingToFile(argv[2]);
        }
        else*/
        sdfObject *moduleObject = moduleToSdfObject(module);
        cout << "Module storing successful" << endl;
        string outputFileName;
        if (argc == 4)
            outputFileName = argv[2];
        else
            outputFileName = avoidNull(module->name) + ".sdf.json";

        moduleObject->objectToFile(outputFileName);
        cout << "Conversion finished" << endl;

        /*//TEST
        sdfObject test("test", "");
        sdfEvent teste("e", "...");
        sdfData testd("d", "...", json_string);
        sdfProperty testp("p", "prop");
        sdfAction testa("testa", "blah");
        sdfData testi("testi", "", "object");
        sdfData op("prop1", "", "string");
        testi.addObjectProperty(&op);
        testa.setInputData(&testi);
        testd.setReference(&op);
        test.addAction(&testa);
        test.addEvent(&teste);
        test.addProperty(&testp);
        testa.addDatatype(&testd);
        testp.setReference(&testd);
        teste.setOutputData(&testd);
        testa.addRequiredInputData(&testd);

        //testd.setStringData("yeah");

        test.objectToFile(argv[2]);*/

        return 0;
    }

    // check whether input file is a SDF file
    if (std::regex_match(argv[1], sdf_json_regex))
    {
        // check whether output file has right format
        // -> only the name of the module is given in the input now
        /*if (!std::regex_match(argv[2], yang_regex))
        {
            printf("Incorrect output file format\n");
            //return -1;
        }*/

        // TODO: fill module
        sdfObject moduleObject;
        sdfThing moduleThing;
        //lys_module *module = new lys_module;
        lys_module module = {};
        //module->ctx = ly_ctx_new(argv[3], 0);
        ly_ctx *ctx = ly_ctx_new(argv[3], 0);
        //module = const_cast<lys_module*>(lys_parse_path(ctx,
        //                "yang/standard/ietf/RFC/ietf-l2vpn-svc.yang",
        //                LYS_IN_YANG));
        if (moduleObject.fileToObject(argv[1]) != NULL)
        {
            //cout
            //<< removeQuotationMarksFromString(moduleObject->objectToString())
            //<< moduleObject->objectToString()
            //<< endl;
            sdfObjectToModule(moduleObject, module);
        }
        else
        {
            moduleThing.fileToThing(argv[1]);
            cout << moduleThing.thingToString() << endl;
            //module = sdfThingToModule(moduleThing);
        }

        string tmpFileName;
        if (argc == 4)
        {
            tmpFileName = argv[2];
            tmpFileName = tmpFileName + ".yang";
            module.name = argv[2];
        }
        else if (argc == 3)
        {
            tmpFileName = module.name;
            tmpFileName = tmpFileName + ".yang";
        }
        const char *fileName = tmpFileName.c_str();

        module.ctx = ctx;
        const lys_module *const_module = const_cast<lys_module*>(&module);
        printf("Made it!\n");

        lys_print_path(fileName, const_module, LYS_OUT_YANG, NULL, 0, 0);

        // validate the model
        lys_parse_path(ctx, fileName, LYS_IN_YANG);

        return 0;
    }

    return -1;
}
