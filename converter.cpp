/*!
 * @file converter.cpp
 * @brief The implementation of header converter.hpp
 *
 * At the core of the converter tool is the piece of software that facilitates
 * the actual conversions. It can generally be split into the two conversion
 * directions SDF to YANG and YANG to SDF.
 */
#include "converter.hpp"

string avoidNull(const char *c)
{
    if (c == NULL)
        return "";
    return string(c);
}

lys_node* storeNode(shared_ptr<lys_node> node)
{
    if (nodeStore.size() == nodeStore.max_size())
    {
        cerr << "storeNode: vector is full, cannot store more nodes" << endl;
        return NULL;
    }
    nodeStore.push_back(node);
    return nodeStore.back().get();
}

const char* storeString(string str)
{
    if (str == "")
        return NULL;

    if (stringStore.size() == stringStore.max_size())
    {
        cerr << "storeString: vector is full, "
                "cannot store more strings" << endl;
        return NULL;
    }

    stringStore.push_back(str);
    return stringStore.back().c_str();
}

lys_restr* storeRestriction(lys_restr restr)
{
    if (restrStore.size() == restrStore.max_size())
    {
        cerr << "storeRestriction: vector is full, "
                "cannot store more restrictions" << endl;
        return NULL;
    }
    restrStore.push_back(restr);
    return &restrStore.back();
}

lys_tpdf* storeTypedef(lys_tpdf *tpdf)
{
    if (tpdfStore.size() == tpdfStore.max_size())
    {
        cerr << "storeTypedef: vector is full, "
                "cannot store more typedefs" << endl;
        return NULL;
    }
    tpdfStore.push_back(tpdf);
    return tpdfStore.back();
}

void* storeVoidPointer(shared_ptr<void> v)
{
    if (voidPointerStore.size() == voidPointerStore.max_size())
    {
        cerr << "storeTvoidPointer: vector is full, "
                "cannot store more void pointers" << endl;
        return NULL;
    }
    voidPointerStore.push_back(v);
    return voidPointerStore.back().get();
}


sdfData* nodeToSdfData(struct lys_node *node, sdfObject *object);

string removeQuotationMarksFromString(string input)
{
    string result = input;
    result.erase(std::remove(result.begin(),
            result.end(), '\"' ), result.end());
    return result;
}

bool typeUsesModule(lys_type *type, lys_module *wanted)
{
    if (!type)
        return false;

    if (type->der && type->der->module
            && strcmp(type->der->module->name, wanted->name) == 0)
    {
        return true;
    }
    if (type->base == LY_TYPE_LEAFREF && type->info.lref.target
            && strcmp(type->info.lref.target->module->name, wanted->name) == 0)
    {
        return true;
    }

    for (int i = 0; type->base == LY_TYPE_IDENT
            && i < type->info.ident.count; i++)
    {
        lys_module * refMod = type->info.ident.ref[i]->module;
        if (refMod && strcmp(refMod->name, wanted->name) == 0)
        {
            return true;
        }
    }
    return false;
}

bool tpdfsUseModule(lys_tpdf *tpdfs, uint16_t tpdfs_size, lys_module *wanted)
{
    lys_tpdf *t;
    for (int i = 0; i < tpdfs_size && tpdfs; i++)
    {
        if (typeUsesModule(&tpdfs[i].type, wanted))
            return true;
    }
    return false;
}

bool identsUseModule(lys_ident *idents, uint16_t ident_size, lys_module *wanted)
{
    lys_ident *id;
    for (int i = 0; i < ident_size; i++)
    {
        id = &idents[i];
        for (int j = 0; j < id->base_size; j++)
        {
            lys_module * baseMod = id->base[j]->module;
            if (baseMod && strcmp(baseMod->name, wanted->name) == 0)
                return true;
        }
    }
    return false;
}

bool subTreeUsesModule(lys_node *node, lys_module *wanted)
{
    if (!node)
        return false;

    lys_node *elem;
    for (elem = node; elem; elem = elem->next)
    {
        if (elem->nodetype == LYS_LEAF
                && typeUsesModule(&((lys_node_leaf*)elem)->type, wanted))
        {
            return true;
        }
        if (elem->nodetype == LYS_LEAFLIST
                && typeUsesModule(&((lys_node_leaflist*)elem)->type, wanted))
        {
            return true;
        }

        if (elem->nodetype == LYS_CONTAINER
                && tpdfsUseModule(((lys_node_container*)elem)->tpdf,
                                  ((lys_node_container*)elem)->tpdf_size,
                                   wanted))
        {
            return true;
        }
        if (elem->nodetype == LYS_LIST
                && tpdfsUseModule(((lys_node_list*)elem)->tpdf,
                                  ((lys_node_list*)elem)->tpdf_size,
                                   wanted))
        {
            return true;
        }
        if (elem->nodetype == LYS_GROUPING
                && tpdfsUseModule(((lys_node_grp*)elem)->tpdf,
                                  ((lys_node_grp*)elem)->tpdf_size,
                                   wanted))
        {
            return true;
        }
        if ((elem->nodetype == LYS_RPC || elem->nodetype == LYS_ACTION)
                && tpdfsUseModule(((lys_node_rpc_action*)elem)->tpdf,
                                  ((lys_node_rpc_action*)elem)->tpdf_size,
                                   wanted))
        {
            return true;
        }
        if ((elem->nodetype == LYS_INPUT || elem->nodetype == LYS_OUTPUT)
                && tpdfsUseModule(((lys_node_inout*)elem)->tpdf,
                                  ((lys_node_inout*)elem)->tpdf_size,
                                   wanted))
        {
            return true;
        }
        if (elem->nodetype == LYS_NOTIF
                && tpdfsUseModule(((lys_node_notif*)elem)->tpdf,
                                  ((lys_node_notif*)elem)->tpdf_size,
                                   wanted))
        {
            return true;
        }

        if (elem->nodetype == LYS_USES
                && ((lys_node_uses*)elem)->grp->module == wanted)
        {
            return true;
        }

        if (subTreeUsesModule(elem->child, wanted))
            return true;
    }

    return false;
}

bool moduleUsesModule(lys_module *module, lys_module *wanted)
{
    if (!wanted)
    {
        cerr << "moduleUsesModule: wanted module is null" << endl;
        return false;
    }

    if (tpdfsUseModule(module->tpdf, module->tpdf_size, wanted))
        return true;

    if (identsUseModule(module->ident, module->ident_size, wanted))
        return true;

    for (int i = 0; i < module->augment_size; i++)
    {
        lys_module *targetMod = module->augment[i].target->module;
        if (targetMod && strcmp(targetMod->name, wanted->name) == 0)
            return true;

        if (subTreeUsesModule(module->augment[i].child, wanted))
            return true;
    }

    return subTreeUsesModule(module->data, wanted);
}

string generatePath(lys_node *node, lys_module *module,
        bool addPrefix)
{
    if (!node)
    {
        cerr << "generatePath: node is null" << endl;
        return "";
    }
    if (!node->module)
    {
        cerr << "generatePath: module is null" << endl;
        return "";
    }

    // Try generating the path automatically
    // (remove prefix if necessary)
    string automatic = "";
    if (!addPrefix //&& !node->module->prefix
                && (!module || node->module == module))
    {
        automatic = avoidNull(lys_path(node, 1));
        smatch sm;
        regex p("/[^/^:]*:(.*)");
        if (regex_match(automatic, sm, p))
            automatic = "/" + sm[1].str();
    }
    else
        automatic = avoidNull(lys_path(node, 0));

//    if (avoidNull(node->name) == "alarm-type-id")
//        cout << "!!!" + automatic << endl;
    if (automatic != "")
        return automatic;

    else
        cerr << "generatePath: automatic path generation failed" << endl;

    // if automatic generation did not work try the next step
    string prefix;
    if ((!addPrefix || !node->module->prefix)
            && (!module || node->module == module))
        prefix = "/";
    else
        prefix = "/" + avoidNull(node->module->prefix) + ":";

    string nodeName = avoidNull(node->name);
    if (nodeName == "" && (node->nodetype & (LYS_INPUT | LYS_OUTPUT)))
    {
        return  generatePath(node->parent, module, addPrefix);
    }

    if (node->parent == NULL)
        return prefix + nodeName;

    return  generatePath(node->parent, module, addPrefix) + prefix + nodeName;
}

/*
 * For a given leaf node that has the type leafref expand the target path
 * given by the leafref so that it does not contain ".." anymore
 * but specifies the full path
 * It is not possible to simply call generatePath on the target in
 * the node info because the target node is not always given
 */
string expandPath(lys_node_leaf *node, bool addPrefix)
{
    if (node == NULL)
    {
        cerr << "expandPath: node is null" << endl;
        return "";
    }
    if (node->type.base != LY_TYPE_LEAFREF)
    {
        cerr << "expandPath: node " + avoidNull(node->name)
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
        for (int i = 0; parent && i < match_up; i++)
            parent = parent->parent;
        path = generatePath(parent, NULL, addPrefix) + "/"
                + string(sm[sm.size()-1]);
    }
    return path;
}
/*
 * Overloaded function
 */
string expandPath(lys_node_leaflist *node, bool addPrefix)
{
    return expandPath((lys_node_leaf*) node, addPrefix);
}
/*
 * Overloaded function
 */
string expandPath(lys_tpdf *tpdf, bool addPrefix)
{
//    lys_node_leaf *node = (lys_node_leaf*)tpdf;
//    node->type = tpdf->type;
//    node->parent = NULL;
//    return expandPath(node, addPrefix);


    if (tpdf == NULL)
    {
        cerr << "expandPath: tpdf is null" << endl;
        return "";
    }
    if (tpdf->type.base != LY_TYPE_LEAFREF)
    {
        cerr << "expandPath: tpdf " + avoidNull(tpdf->name)
                + "  has to be of base type leafref" << endl;
        return "";
    }
    return avoidNull(tpdf->type.info.lref.path);
//    string path = avoidNull(tpdf->type.info.lref.path);
//    lys_node *parent = (lys_node*)node;
//    smatch sm;
//    regex all_regex("(\\.\\./)+(.*)");
//    regex up_regex("\\.\\./");
//    if (regex_match(path, sm, all_regex))
//    {
//        auto begin_up = sregex_iterator(path.begin(), path.end(), up_regex);
//        auto end_up = sregex_iterator();
//        int match_up = distance(begin_up, end_up);
//        for (int i = 0; parent && i < match_up; i++)
//            parent = parent->parent;
//        path = generatePath(parent, NULL, addPrefix) + "/"
//                + string(sm[sm.size()-1]);
//    }
//    return path;
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
    else if (type == LY_TYPE_STRING || type == LY_TYPE_ENUM)
        return json_string;
    else
    {
        // error handling?
        return json_type_undef;
    }
}

LY_DATA_TYPE stringToLType(std::string type)
{
    if (type == "string")
        return LY_TYPE_STRING;
    else if (type == "number" || type == "dec64")
        return LY_TYPE_DEC64;
    else if (type == "boolean")
        return LY_TYPE_BOOL;
    else if (type == "integer" || type == "int64")
        return LY_TYPE_INT64;
    else if (type == "int32")
        return LY_TYPE_INT32;
    else if (type == "int16")
        return LY_TYPE_INT16;
    else if (type == "int8")
        return LY_TYPE_INT8;
    else if (type == "uint64")
        return LY_TYPE_UINT64;
    else if (type == "uint32")
        return LY_TYPE_UINT32;
    else if (type == "uint16")
        return LY_TYPE_UINT16;
    else if (type == "uint8")
        return LY_TYPE_UINT8;
    else if (type == "ident" || type == "identity" || type == "identityref")
        return LY_TYPE_IDENT;
    else if (type == "union")
        return LY_TYPE_UNION;
    else if (type == "binary")
        return LY_TYPE_BINARY;
    else if (type == "bits")
        return LY_TYPE_BITS;
    else if (type == "empty")
        return LY_TYPE_EMPTY;
    else if (type == "instance-identifier")
        return LY_TYPE_INST;
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
    else if (type->base == LY_TYPE_STRING || type->base == LY_TYPE_ENUM)
        return "string";
    else
    {
        return "";
    }
}

void addOriginNote(sdfCommon *com, string stmt, string arg)
{
    string dsc = com->getDescription(), note = stmt;
    if (arg != "")
        note += " " + arg;

    if (dsc == "")
        dsc = "!Conversion note: " + note + "!\n";
    else
        dsc += "\n!Conversion note: " + note + "!\n";

    com->setDescription(dsc);
}

vector<int64_t> rangeToInt(const char *range)
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
            return {stoi(min), stoi(max)};

        if (min != "min" && max == "max")
            return {stoi(min), MAX_INT};

        if (min == "min" && max != "max")
            return {MIN_INT, stoi(max)};

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

        vector<int64_t> result = {};
        while (iter != end)
        {
            string match = (*iter)[0].str();
            for (float value : rangeToInt(match.c_str()))
                result.push_back(value);
            iter++;
        }
        return result;
    }

    // if the string consists of a single number instead of a range
    if (str == "min")
        return {MIN_INT, MIN_INT};
    if (str == "max")
        return {MAX_INT, MAX_INT};

    try
    {
        return {stoi(str), stoi(str)};
    }
    catch (std::invalid_argument&)
    {
        cerr << "range string '" + str + "' is not a correct range"
                << endl;
    }
    return {};
}


/*
 * Uses regex to take apart range strings (e.g. "0..1" to 0 and 1)
 */
vector<float> rangeToFloat(const char *range)
{
    if (!range || strcmp(range, "") == 0)
        return {};

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

const char* floatToRange(float min, float max)
{
    if (isnan(min) && isnan(max))
        return NULL;
    if (min == max)
    {
        return storeString(to_string(min));
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

    return storeString(first + ".." + second);
}

const char* intToRange(int64_t min, uint64_t max)
{
    if (min == max)
    {
        return storeString(to_string(min));
    }
    string first = to_string(min);
    string second = to_string(max);

    return storeString(first + ".." + second);
}

string statusToDescription(uint16_t flags, string dsc)
{
    if ((flags & LYS_STATUS_MASK) == LYS_STATUS_DEPRC)
        return "!Conversion note: status DEPRECATED!\n" + dsc;
    else if ((flags & LYS_STATUS_MASK) == LYS_STATUS_OBSLT)
        return "!Conversion note: status OBSOLETE!\n" + dsc;
    else if ((flags & LYS_STATUS_MASK) == LYS_STATUS_CURR)
        return "!Conversion note: status CURRENT!\n" + dsc;
    return dsc;
}

string mustToDescription(lys_restr *must, int size, string dsc)
{
    string result = dsc;

    for (int i = 0; i < size; i++)
    {
        if (result == "")
            result = "!Conversion note: must " + avoidNull(must[i].expr)
                            + "!\n";
        else
            result += "\n!Conversion note: must " + avoidNull(must[i].expr)
                            + "!\n";
    }

    dsc = result;
    return result;
}

string whenToDescription(lys_when *when, string dsc)
{
    string whenString = "";
    if (when)
    {
        if (when->cond)
            whenString += "!Conversion note: when " + string(when->cond)
                          + "!\n";
    }
    if (dsc != "")
        return dsc + "\n\n" + whenString;
    else return whenString;
}

/*
 *  Information from the given lys_type struct is translated
 *  and set accordingly in the given sdfData element
 */
sdfData* typeToSdfData(struct lys_type *type, sdfData *data,
        bool parentIsTpdf)
{
    if (!type)
    {
        cerr << "typeToSdfData: given lys_type pointer must not be NULL"
                << endl;
        return data;
    }

    if (type->base == LY_TYPE_LEAFREF
            && strcmp(type->der->name, "leafref") == 0)
    {
        // what if instead of using the path, the target node would be
        // stored with the corresponding data element somehow?
        data->setType("");
        if (type->info.lref.target)
        {
            referencesLeft.push_back(tuple<string, string, sdfCommon*>{
                   generatePath((lys_node*)type->info.lref.target),
                   generatePath((lys_node*)type->info.lref.target, NULL, true),
                   (sdfCommon*)data});
        }
        else
        {
            if (parentIsTpdf)
            {
                referencesLeft.push_back(tuple<string, string, sdfCommon*>{
                    expandPath(type->parent),
                    expandPath(type->parent, true),
                    (sdfCommon*)data});
            }
            else
            {
                referencesLeft.push_back(tuple<string, string, sdfCommon*>{
                    expandPath((lys_node_leaf*)type->parent),
                    expandPath((lys_node_leaf*)type->parent, true),
                    (sdfCommon*)data});
            }
        }
    }
    else if (strcmp(type->der->name, "empty") == 0)
    {
        data->setSimpType(json_object);
        addOriginNote(data, "type", "empty");
    }
    else if (strcmp(type->der->name, "bits") == 0)
    {
        data->setSimpType(json_object);
        sdfData *bitProp;
        for (int i = 0; i < type->info.bits.count; i++)
        {
            bitProp = new sdfData();
            bitProp->setName(avoidNull(type->info.bits.bit[i].name));
            string dsc = "Bit at position "
                            + to_string(type->info.bits.bit[i].pos);
            if (type->info.bits.bit[i].dsc)
                dsc += ": " + avoidNull(type->info.bits.bit[i].dsc);
            if (type->info.bits.bit[i].ref)
                dsc += "\n" + avoidNull(type->info.bits.bit[i].ref);
            bitProp->setDescription(dsc);
            bitProp->setType(json_boolean);
            data->addObjectProperty(bitProp);
        }
        addOriginNote(data, "type", "bits");
    }
    else if (strcmp(type->der->name, "binary") == 0)
    {
        data->setType(json_string);
        data->setSubtype(sdf_byte_string);
        addOriginNote(data, "type", "binary");
        vector<float> min_max;
        if (type->info.binary.length != NULL)
            min_max = rangeToFloat(type->info.binary.length->expr);

        for (int i = 0; !min_max.empty() && i < min_max.size()-1; i = i+2)
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
    }
    else if (strcmp(type->der->name, "union") == 0)
    {
        sdfData *choice;
        for (int i = 0; i < type->info.uni.count; i++)
        {
            choice = new sdfData();
            choice->setName(avoidNull(type->info.uni.types[i].der->name));
            choice->setType(stringToJsonDType(
                    type->info.uni.types[i].der->name));
            data->addChoice(choice);
            typeToSdfData(&type->info.uni.types[i], choice);
            addOriginNote(data, "type", "union");
        }
    }
    else if (strcmp(type->der->name, "instance-identifier") == 0)
    {
        // round trip?
        data->setType(json_string);
        addOriginNote(data, "type", "instance-identifier");
    }
    else if (stringToJsonDType(type->der->name) == json_type_undef
            && data->getSimpType() != json_array
            && strcmp(type->der->name, "enumeration") != 0
            && strcmp(type->der->name, "identityref") != 0)
    {
        typerefs.push_back(tuple<string, string, sdfCommon*>{
           avoidNull(type->der->name),
           avoidNull(type->parent->module->prefix) + ":"
               + avoidNull(type->der->name),
           data});
    }
    if (type->base == LY_TYPE_IDENT
            && strcmp(type->der->name, "identityref") == 0)
    {
        if (type->info.ident.count > 1)
        {
            for (int i = 0; i < type->info.ident.count; i++)
            {
                sdfData *ref = new sdfData(
                        avoidNull(type->info.ident.ref[i]->name),
                        avoidNull(type->info.ident.ref[i]->dsc),
                        json_type_undef);

                if (identities[avoidNull(type->info.ident.ref[i]->name)])
                    ref->setReference(
                          identities[avoidNull(type->info.ident.ref[i]->name)]);
                else
                {
                    identsLeft.push_back(tuple<string, string, sdfCommon*>{
                        avoidNull(type->info.ident.ref[i]->name),
                        avoidNull(type->parent->module->prefix) + ":"
                            + avoidNull(type->info.ident.ref[i]->name),
                        //(sdfCommon*)data});
                        (sdfCommon*)ref});

                }
                data->setSimpType(json_object);
                data->addObjectProperty(ref);
            }
        }
        else if (type->info.ident.count == 1)
        {
            data->setType(json_type_undef);
            if (identities[avoidNull(type->info.ident.ref[0]->name)])
                data->setReference(
                        identities[avoidNull(type->info.ident.ref[0]->name)]);
            else
            {
                identsLeft.push_back(tuple<string, string, sdfCommon*>{
                    avoidNull(type->info.ident.ref[0]->name),
                    avoidNull(type->parent->module->prefix) + ":"
                        + avoidNull(type->info.ident.ref[0]->name),
                    (sdfCommon*)data});
            }
        }
        else if (type->info.ident.count < 1)
        {
            cerr << "typeToSdfData: element '" + data->getName()
            + "' with LY_TYPE_IDENT has <1 identityrefs" << endl;

            string dsc = data->getDescription();
            dsc += "\nLibyang did not parse the identityref definition(s) of "
                    "this element\n";
            data->setDescription(dsc);
        }

        addOriginNote(data, "type", "identityref");
    }

    else if (type->base == LY_TYPE_ENUM)
        data->setType(json_string);

    jsonDataType jsonType = data->getSimpType();
    // check for types and translate accordingly
    // number
    if (jsonType == json_number)
    {
        //data->setMultipleOf(type->info.dec64.div);
        data->setMultipleOf(1 / pow(10, type->info.dec64.dig));
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
        string cnst = "";

        // second condition is a hot fix
        if (type->info.str.pat_count > 0 && type->base == LY_TYPE_STRING)
        {
            //pattern = type->info.str.patterns[0].expr;
            //data->setPattern(pattern);
            if (type->info.str.pat_count == 1)
            {
                // the first byte of expr is ignored because it contains
                // information about the match type (regular or invert)
                string patt = avoidNull(type->info.str.patterns[0].expr+1);
                if (type->info.str.patterns[0].expr[0] == 0x15)
                    patt = "((?!(" + patt + ")).)*";

                data->setPattern(patt);

                if (type->info.str.patterns[0].dsc
                        || type->info.str.patterns[0].ref)
                    data->setDescription(data->getDescription()
                            + "\n\n!Conversion node: pattern "
                            + avoidNull(type->info.str.patterns[0].dsc)
                            + "\n!Conversion node: patternref  "
                            + avoidNull(type->info.str.patterns[0].ref));
            }

            else // patterns are ANDed not ORed
                // -> how can this be represented in SDF?
            {
                string combPattern = "";
                for (int i = 0; i < type->info.str.pat_count; i++)
                {
                    string patt = avoidNull(type->info.str.patterns[i].expr+1);
                    if (type->info.str.patterns[i].expr[0] == 0x15)
                    {
                        patt = "((?!" + patt + ").*)";
                        addOriginNote(data, "pattern-invert-match",
                                avoidNull(type->info.str.patterns[i].expr+1));
                    }
                    else
                        addOriginNote(data, "pattern",
                                avoidNull(type->info.str.patterns[i].expr+1));

                    if (i < type->info.str.pat_count - 1)
                        combPattern += "(?=" + patt + ")";
                    else
                        combPattern += patt;
                }
                data->setPattern(combPattern);
            }
        }

        if (type->base == LY_TYPE_ENUM)
        {
            vector<string> enum_names = {};
            for (int i = 0; i < type->info.enums.count; i++)
            {
                enum_names.push_back(avoidNull(type->info.enums.enm[i].name));
                if (type->info.enums.enm[i].dsc)
                {
                    data->setDescription(data->getDescription() + "\n"
                        + avoidNull(type->info.enums.enm[i].name) + ": "
                        + avoidNull(type->info.enums.enm[i].dsc));
                }
            }
            data->setEnumString(enum_names);
        }
        // The second condition is a hot fix
        else if (type->info.str.length && type->base == LY_TYPE_STRING)
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
        }
    }

    // boolean
    else if (jsonType == json_boolean)
    {
        data->setBoolData(false, false, false, false);
    }

    // integer
    else if (jsonType == json_integer)
    {
        // set conversion not to specific type
        addOriginNote(data, "type", avoidNull(type->der->name));
        // if no range is given, set the min and max according to int type
        if (avoidNull(type->der->name) == "int64")
        {
            data->setMinInt(-9223372036854775808U);
            data->setMaxInt(9223372036854775807);
        }
        else if (avoidNull(type->der->name) == "uint64")
        {
            data->setMinInt(0);
            data->setMaxInt(18446744073709551615U);
        }
        else if (avoidNull(type->der->name) == "int32")
        {
            data->setMinInt(-2147483648);
            data->setMaxInt(2147483647);
        }
        else if (avoidNull(type->der->name) == "uint32")
        {
            data->setMinInt(0);
            data->setMaxInt(4294967295);
        }
        else if (avoidNull(type->der->name) == "int16")
        {
            data->setMinInt(-32768);
            data->setMaxInt(32767);
        }
        else if (avoidNull(type->der->name) == "uint16")
        {
            data->setMinInt(0);
            data->setMaxInt(65535);
        }
        else if (avoidNull(type->der->name) == "int8")
        {
            data->setMinInt(-128);
            data->setMaxInt(127);
        }
        else if (avoidNull(type->der->name) == "uint8")
        {
            data->setMinInt(0);
            data->setMaxInt(255);
        }

        if (type->info.num.range != NULL)
        {
            vector<int64_t> min_max
                = rangeToInt(type->info.num.range->expr);

            for (int i = 0; i < min_max.size()-1; i = i+2)
            {
                sdfData *d;
                if (min_max.size() > 2)
                {
                    d = new sdfData("range_option_" + to_string(i/2), "",
                            json_type_undef);
                    data->addChoice(d);
                    data->eraseMaxInt();
                    data->eraseMinInt();
                    data->setMinimum(nan(""));
                    data->setMaximum(nan(""));
                }
                else
                    d = data;

                if (min_max[i] == min_max[i+1])
                {
                    d->setConstantInt(min_max[i]);
                }
                else
                {
                    d->setMinInt(min_max[i]);
                    d->setMaxInt(min_max[i+1]);
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
    if (!tpdf)
    {
        cerr << "typedefToSdfData: given lys_tpdf pointer must not be NULL"
                << endl;
        return NULL;
    }
    sdfData *data = new sdfData(avoidNull(tpdf->name), avoidNull(tpdf->dsc),
            parseTypeToString(&tpdf->type));

    // translate the status of the typedef
    data->setDescription(statusToDescription(tpdf->flags,
            data->getDescription()));

    addOriginNote(data, "typedef");

    // translate units
    string units = "";
    if (tpdf->units != NULL)
    {
        data->setUnits(tpdf->units);
    }

    typeToSdfData(&tpdf->type, data, true);
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
        sdfObject *object)
{
    sdfProperty *property = new sdfProperty(avoidNull(node->name),
            avoidNull(node->dsc),
            parseBaseType(node->type.base));

    // translate the status of the leaf
    property->setDescription(statusToDescription(node->flags,
            property->getDescription()));

    if (node->ref)
        addOriginNote(property, "reference", avoidNull(node->ref));

    if ((node->flags & LYS_CONFIG_W) && (node->flags & LYS_CONFIG_SET))
        property->setWritable(true);
    if ((node->flags & LYS_CONFIG_R) && (node->flags & LYS_CONFIG_SET))
        property->setReadable(true);

    // if feature
    string iffString = "";
    int eIndex = 0, fIndex = 0;
    if (node->iffeature_size > 0 && node->iffeature->expr
            && node->iffeature->features[0])
    {
        iffString = resolve_iffeature_recursive(node->iffeature, &eIndex,
                &fIndex);
        addOriginNote(property, "if feature", iffString);
    }

    typeToSdfData(&node->type, property);
    // save reference to leaf for ability to convert leafref-type
    if (node->type.base != LY_TYPE_LEAFREF
            && strcmp(node->type.der->name, "leafref") != 0)
        leafs[generatePath((lys_node*)node)] = property;

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

    for (int i = 0; i < node->ext_size; i++)
    {
        addOriginNote(property, avoidNull(node->ext[i]->def->name),
                avoidNull(node->ext[i]->arg_value));
    }

    return property;
}

sdfData* leafToSdfData(struct lys_node_leaf *node,
        sdfObject *object)
{
    sdfData *data = new sdfData(*leafToSdfProperty(node, object));

    if (node->type.base == LY_TYPE_LEAFREF
            && strcmp(node->type.der->name, "leafref") == 0)
    {
        if (node->type.info.lref.target)
        {
            referencesLeft.push_back(tuple<string, string, sdfCommon*>{
               generatePath((lys_node*)node->type.info.lref.target),
               generatePath((lys_node*)node->type.info.lref.target, NULL, true),
               (sdfCommon*)data});
        }
        else
        {
            referencesLeft.push_back(tuple<string, string, sdfCommon*>{
               expandPath(node), expandPath(node, true), (sdfCommon*)data});
        }
    }
    // find what leafToSdfProperty returns in typerefs and overwrite with data
    else
    {
        if (stringToJsonDType(node->type.der->name) == json_type_undef
                && strcmp(node->type.der->name, "enumeration") != 0
                && strcmp(node->type.der->name, "identityref") != 0
                && strcmp(node->type.der->name, "empty") != 0
                && strcmp(node->type.der->name, "union") != 0
                && strcmp(node->type.der->name, "bits") != 0
                && strcmp(node->type.der->name, "binary") != 0)
        {
            typerefs.push_back(tuple<string, string, sdfCommon*>{
                avoidNull(node->type.der->name),
                avoidNull(node->module->prefix) + ":"
                    + avoidNull(node->type.der->name),
                data});
        }

        leafs[generatePath((lys_node*)node)] = data;
        leafs[generatePath((lys_node*)node, NULL, true)] = data;
    }
    assert(!dynamic_cast<sdfProperty*>(data));
    return data;
}

/*
 * The information is extracted from the given lys_node_leaflist struct and
 * a corresponding sdfProperty object is generated
 */
sdfProperty* leaflistToSdfProperty(struct lys_node_leaflist *node,
        sdfObject *object)
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

    // overwrite reference for path
    if (node->type.base != LY_TYPE_LEAFREF)
        leafs[generatePath((lys_node*)node)] = property;

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
        sdfObject *object)
{
    sdfData *data = new sdfData(*leaflistToSdfProperty(node, object));

    if (node->type.base != LY_TYPE_LEAFREF)
       leafs[generatePath((lys_node*)node)] = data;

    assert(!dynamic_cast<sdfProperty*>(data));
    return data;
}

/*
 * The information is extracted from the given lys_node_container struct and
 * a corresponding sdfThing object is generated

sdfThing* containerToSdfThing(struct lys_node_container *node)
{
    sdfThing *thing = new sdfThing(avoidNull(node->name),
            avoidNull(node->dsc));
    return thing;
} */


sdfAction* actionToSdfAction(lys_node_rpc_action *node, sdfObject *object,
        sdfData *data)
{
    sdfAction *action = new sdfAction(avoidNull(node->name),
            avoidNull(node->dsc));
    if (node->ref != NULL)
        addOriginNote(data, "reference", avoidNull(node->ref));

    // translate the status of the notification
    action->setDescription(statusToDescription(node->flags,
            action->getDescription()));

    for (int i = 0; i < node->ext_size; i++)
    {
        addOriginNote(action, avoidNull(node->ext[i]->def->name),
                avoidNull(node->ext[i]->arg_value));
    }

    for (int i = 0; i < node->tpdf_size; i++)
        action->addDatatype(typedefToSdfData(&node->tpdf[i]));

    // if feature
    string iffString = "";
    int eIndex = 0, fIndex = 0;
    if (node->iffeature_size > 0 && node->iffeature->expr
            && node->iffeature->features[0])
    {
        iffString = resolve_iffeature_recursive(node->iffeature, &eIndex,
                &fIndex);
        addOriginNote(data, "if feature", iffString);
    }

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
                sdfData *inputPropsInlay = new sdfData(*data);
                inputProps->setName(action->getName());
                inputPropsInlay->addObjectProperty(inputProps);
                // What if data is of type array? translation of list?
                // -> not a problem because data is still the itemConstr of
                // the list->array at this point which is of type object
                input->addObjectProperty(inputPropsInlay);
                // is the eliciting node->data required in the input?
                input->addRequiredObjectProperty(inputPropsInlay->getName());

                input->setDescription(
                        mustToDescription(
                            ((lys_node_inout*)node)->must,
                            ((lys_node_inout*)node)->must_size,
                            input->getDescription()));

                action->setInputData(input);
            }
            else
            {
                inputProps->setDescription(
                        mustToDescription(
                            ((lys_node_inout*)node)->must,
                            ((lys_node_inout*)node)->must_size,
                            inputProps->getDescription()));
                action->setInputData(inputProps);
            }

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
             * If the child node is an 'only child' should not it be
             * translated to object properties of the output but to output
             * directly?
             */
            action->setOutputData(output);
        }
    }

    return action;
}

string sdfSpecExtToString(lys_ext_instance **ext, int ext_size)
{
    if (!ext)
        return "";

    // read out the original sdf spec from the extension if there is one
    for (int i = 0; i < ext_size; i++)
    {
        if (ext[i] && ext[i]->def && avoidNull(ext[i]->def->name) == "sdf-spec")
            return avoidNull(ext[i]->arg_value);
    }
    return "";
}

uint8_t iff_getop(uint8_t *list, int pos)
{
    uint8_t *item;
    uint8_t mask = 3, result;

    assert(pos >= 0);

    item = &list[pos / 4];
    result = (*item) & (mask << 2 * (pos % 4));
    return result >> 2 * (pos % 4);
}
string resolve_iffeature_recursive(struct lys_iffeature *expr, int *index_e,
        int *index_f)
{
    uint8_t op;
    string a, b;

    op = iff_getop(expr->expr, *index_e);
    (*index_e)++;

    switch (op) {
    case LYS_IFF_F:
        /* resolve feature */
        return avoidNull(expr->features[(*index_f)++]->name);
    case LYS_IFF_NOT:
        /* invert result */
        return "not " + resolve_iffeature_recursive(expr, index_e, index_f);
    case LYS_IFF_AND:
    case LYS_IFF_OR:
        a = resolve_iffeature_recursive(expr, index_e, index_f);
        b = resolve_iffeature_recursive(expr, index_e, index_f);
        if (op == LYS_IFF_AND) {
            return a + " and " + b;
        } else { /* LYS_IFF_OR */
            return a + " or " + b;
        }
    }

    return "";
}

sdfData* nodeToSdfData(struct lys_node *node, sdfObject *object)
{
    if (!node)
        return NULL;

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
    if (node->ref)
        addOriginNote(data, "reference", avoidNull(node->ref));

    for (int i = 0; i < node->ext_size; i++)
    {
        if (avoidNull(node->ext[i]->def->name) != "sdf-spec")
                addOriginNote(data, avoidNull(node->ext[i]->def->name),
                        avoidNull(node->ext[i]->arg_value));
    }

    // translate the status of the node
    data->setDescription(statusToDescription(node->flags,
            data->getDescription()));

    // if feature
    string iffString = "";
    int eIndex = 0, fIndex = 0;
    if (node->iffeature_size > 0 && node->iffeature->expr
            && node->iffeature->features[0])
    {
        iffString = resolve_iffeature_recursive(node->iffeature, &eIndex,
                &fIndex);
        addOriginNote(data, "if feature", iffString);
    }

    string origin = sdfSpecExtToString(node->ext, node->ext_size);
    // this only add to the object's sdfRequired which might not always
    // be how it originally was
    if (origin == "sdfRequired")
        object->addRequired(data);

    if ((node->flags & LYS_CONFIG_W) && (node->flags & LYS_CONFIG_SET))
        data->setWritable(true);
    if ((node->flags & LYS_CONFIG_R) && (node->flags & LYS_CONFIG_SET))
        data->setReadable(true);

    // translate the when and must statement (if applicable)
    lys_when *when = NULL;
    lys_restr *must = NULL;
    uint8_t mustSize = 0;
    if (node->nodetype & LYS_CONTAINER)
    {
        when = ((lys_node_container*)node)->when;
        must = ((lys_node_container*)node)->must;
        mustSize = ((lys_node_container*)node)->must_size;
        mustToDescription(must, mustSize, avoidNull(node->dsc));
    }
    else if (node->nodetype & LYS_LEAFLIST)
    {
        must = ((lys_node_leaflist*)node)->must;
        mustSize = ((lys_node_leaflist*)node)->must_size;
        mustToDescription(must, mustSize, avoidNull(node->dsc));
    }
    else if (node->nodetype & LYS_NOTIF)
    {
        must = ((lys_node_notif*)node)->must;
        mustSize = ((lys_node_notif*)node)->must_size;
        mustToDescription(must, mustSize, avoidNull(node->dsc));
    }
    else if (node->nodetype & LYS_LIST)
    {
        must = ((lys_node_list*)node)->must;
        mustSize = ((lys_node_list*)node)->must_size;
        mustToDescription(must, mustSize, avoidNull(node->dsc));
    }
    else if (node->nodetype & LYS_CHOICE)
        when = ((lys_node_choice*)node)->when;
    else if (node->nodetype & LYS_ANYDATA)
        when = ((lys_node_anydata*)node)->when;
    else if (node->nodetype & LYS_CASE)
        when = ((lys_node_case*)node)->when;
    if (when)
        addOriginNote(data, "when", when->cond);

    sdfCommon *com;
    // iterate over all child nodes
    for (lys_node *elem = node->child; elem; elem = elem->next)
    {
        // if the node is marked to be ignored
        if (node->flags & IGNORE_NODE)
            continue;

        // conversion note for anydata
        if (elem->nodetype == LYS_ANYDATA)
            addOriginNote(data, "anydata", avoidNull(elem->name));
        // conversion note for anyxml
        else if (elem->nodetype == LYS_ANYXML)
            addOriginNote(data, "anyxml", avoidNull(elem->name));

        // Actions, unlike RPCs, can be tied to containers or lists
        else if (elem->nodetype == LYS_ACTION)
        {
            sdfAction *action = actionToSdfAction(
                    (lys_node_rpc_action*)elem, object, data);
            action->setDescription(
                    "Action connected to " + avoidNull(node->name) + "\n\n"
                    + action->getDescription());
            object->addAction(action);
            com = action;
        }
        // YANG modules cannot have actions at the top level, only rpcs
        else if (elem->nodetype == LYS_RPC)
        {
            // rpcs are converted to sdfAction
            sdfAction *action = actionToSdfAction(
                    (lys_node_rpc_action*)elem, object);
            object->addAction(action);

            com = action;
        }
        else if (elem->nodetype == LYS_CASE)
        {
            sdfData *c = nodeToSdfData(elem, object);
            data->addChoice(c);
            branchRefs[generatePath(elem)] = c;

            c->setDescription(whenToDescription(
                    ((lys_node_case*)elem)->when, c->getDescription()));
            com = c;
        }
        else if (elem->nodetype == LYS_CHOICE)
        {
            sdfData *choiceData = nodeToSdfData(elem, object);

            for (sdfData *prop : choiceData->getObjectProperties())
                choiceData->addChoice(prop);
            choiceData->setObjectProperties({});

            data->addObjectProperty(choiceData);
            if (elem->flags && LYS_MAND_MASK == LYS_MAND_TRUE)
                object->addRequired((sdfCommon*)choiceData);

            // conversion note for default
            if (((lys_node_choice*)elem)->dflt)
            {
                addOriginNote(choiceData, "default",
                        ((lys_node_choice*)elem)->dflt->name);
            }

            choiceData->setDescription(whenToDescription(
                 ((lys_node_choice*)elem)->when, choiceData->getDescription()));
            com = choiceData;
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

            if (((lys_node_container*)elem)->presence)
                addOriginNote(container, "presence",
                        avoidNull(((lys_node_container*)elem)->presence));
            com = container;
        }
        else if (elem->nodetype == LYS_GROUPING)
        {
            sdfData *datatype = nodeToSdfData(elem, object);
            object->addDatatype(datatype);
            branchRefs[generatePath(elem)] = datatype;
            com = datatype;
        }
        else if (elem->nodetype == LYS_LEAF)
        {
            sdfData *next = leafToSdfData((lys_node_leaf*)elem, object);

            if ((elem->flags & LYS_UNIQUE) == LYS_UNIQUE)
                addOriginNote(next, "unique");

            data->addObjectProperty(next);

            if ((elem->flags & LYS_MAND_MASK) == LYS_MAND_TRUE)
                data->addRequiredObjectProperty(avoidNull(elem->name));

            com = next;
        }
        else if (elem->nodetype == LYS_LEAFLIST)
        {
            sdfData *next = leaflistToSdfData((lys_node_leaflist*)elem, object);
            data->addObjectProperty(next);

            for (int i = 0; i < ((lys_node_leaflist*)elem)->must_size; i++)
            {
                addOriginNote(next, "must",
                        avoidNull(((lys_node_leaflist*)elem)->must[i].expr));
            }

            if (elem->flags & LYS_USERORDERED)
                addOriginNote(next, "ordered-by", "user");

            com = next;
        }
        else if (elem->nodetype == LYS_LIST)
        {
            sdfData *next = new sdfData(avoidNull(elem->name),
                    avoidNull(elem->dsc), json_array);

            // translate the status of the list node
            next->setDescription(statusToDescription(elem->flags,
                    next->getDescription()));

            for (int i = 0; i < elem->ext_size; i++)
            {
                addOriginNote(next, avoidNull(elem->ext[i]->def->name),
                        avoidNull(elem->ext[i]->arg_value));
            }

            if (((lys_node_list*)elem)->when)
            {
                addOriginNote(next, "when",
                        avoidNull(((lys_node_list*)elem)->when->cond));
            }
            for (int i = 0; i < ((lys_node_list*)elem)->must_size; i++)
            {
                addOriginNote(next, "must",
                        avoidNull(((lys_node_list*)elem)->must[i].expr));
            }

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

            // look for artificial keys, do not round trip them
            bool artKey;
            for (int i = 0; i < ((lys_node_list*)elem)->keys_size; i++)
            {
                artKey = false;
                for (int j = 0; j < ((lys_node_list*)elem)->keys[i]->ext_size;
                        j++)
                {
                    if (avoidNull(
                            ((lys_node_list*)elem)->keys[i]->ext[j]->arg_value)
                            == "artificial-key")
                        artKey = true;
                }
                if (!artKey)
                    addOriginNote(next, "key",
                        avoidNull(((lys_node_list*)elem)->keys[i]->name));
            }

            if (elem->flags & LYS_USERORDERED)
                addOriginNote(next, "ordered-by", "user");

            // look for unique leaf nodes in the sub-tree
            lys_node *after, *now;
            LY_TREE_DFS_BEGIN(elem, after, now)
            {
                if (now->nodetype == LYS_LEAF && (now->flags & LYS_UNIQUE))
                    next->setUniqueItems(true);
                LY_TREE_DFS_END(elem, after, now);
            }


            data->addObjectProperty(next);
            com = next;
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
            // Transfer extensions
            for (int i = 0; i < elem->ext_size; i++)
            {
                addOriginNote(event, avoidNull(elem->ext[i]->def->name),
                        avoidNull(elem->ext[i]->arg_value));
            }

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

            com = event;
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
                    avoidNull(elem->dsc), "");
                referencesLeft.push_back(tuple<string, string, sdfCommon*>{
                    generatePath((lys_node*)((lys_node_uses*)elem)->grp),
                    generatePath((lys_node*)((lys_node_uses*)elem)->grp, NULL,
                            true),
                   (sdfCommon*)uses});

                // remove the prefix on the name of the uses if there is one
                string name = uses->getName();
                smatch sm;
                regex r("[^:]*:(.*)");
                if(regex_match(name, sm, r))
                    uses->setName(sm[1].str());

                // translate the status of the uses node
                uses->setDescription(statusToDescription(elem->flags,
                        uses->getDescription()));
            }

            if (((lys_node_uses*)elem)->when)
            {
                addOriginNote(uses, "when",
                        avoidNull(((lys_node_uses*)elem)->when->cond));
            }

            data->addObjectProperty(uses);
            com = uses;
        }

        if (elem->parent && elem->parent->nodetype == LYS_AUGMENT)
        {
            addOriginNote(com, "augment-by",
                    avoidNull(elem->parent->module->name));
        }


        // if feature
        iffString = "";
        eIndex = 0;
        fIndex = 0;
        if (elem->iffeature_size > 0 && elem->iffeature->expr
                && elem->iffeature->features[0])
        {
            iffString = resolve_iffeature_recursive(elem->iffeature, &eIndex,
                    &fIndex);
            addOriginNote(com, "if feature", iffString);
        }
        // remove sdf-spec
        for (int i = 0; i < elem->ext_size; i++)
        {
            if (avoidNull(elem->ext[i]->def->name) != "sdf-spec")
                    addOriginNote(com, avoidNull(elem->ext[i]->def->name),
                            avoidNull(elem->ext[i]->arg_value));
        }
    }
    // really add tpdfs to object or rather to last parent property etc?
    // -> yes for now
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

    addOriginNote(ident, "identity");

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
                identsLeft.push_back(tuple<string, string, sdfCommon*>{
                    _ident.base[j]->name,
                    avoidNull(_ident.module->prefix) + ":"
                        + avoidNull(_ident.base[j]->name),
                    (sdfCommon*)ref});
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
            identsLeft.push_back(tuple<string, string, sdfCommon*>{
                _ident.base[0]->name,
                avoidNull(_ident.module->prefix) + ":"
                    + avoidNull(_ident.base[0]->name),
                (sdfCommon*)ident});
        }
    }

    return ident;
}

vector<tuple<string, string, sdfCommon*>> assignReferences(
        vector<tuple<string, string, sdfCommon*>> refsLeft,
        map<string, sdfCommon*> refs)
{
    // check for references left unassigned
    string str, strWRef;
    sdfCommon *com;
    vector<tuple<string, string, sdfCommon*>> stillLeft = {};

    for (tuple<string, string, sdfCommon*> r : refsLeft)
    {
        tie(str, strWRef, com) = r;
        if (com && refs[str])
            com->setReference(refs[str]);
        else if (com && refs[strWRef])
            com->setReference(refs[strWRef]);
        // hot fix for leafrefs where only a path and no target node is
        // defined
        else if (com && refs["/buffer" + str])
            com->setReference(refs["/buffer" + str]);
        else
        {
            // Try to remove the prefix
            smatch sm;
            regex withPrefix("/([^/^:]+):([^/^:]+)");
            string strNoPrefix = "", search = str, prefix = "";
            while (regex_search(search, sm, withPrefix))
            {
                strNoPrefix += "/" + sm[2].str();
                prefix =  sm[1].str() + ":";
                search = sm.suffix().str();
            }
            if (com && refs[strNoPrefix])
                com->setReference(refs[strNoPrefix]);
            // hot fix, see above
            else if (com && refs["/buffer" + strNoPrefix])
                com->setReference(refs["/buffer" + strNoPrefix]);
            else if (com && refs["/" + prefix + "buffer" + strWRef])
                com->setReference(refs["/" + prefix + "buffer" + strWRef]);
            else
            {
                stillLeft.push_back(r);
                if (com)
                    cout << "assignReferences: no reference found for "
                        + str + " of " + com->getName() << endl;
                else
                    cout << "assignReferences: no reference found for "
                    + strWRef + " of (null)" << endl;
            }
        }

        // add the namespace of the reference if necessary
        sdfCommon *ref = com->getReference();
        sdfFile *topFile = com->getTopLevelFile();

        if (ref && topFile)
        {
            bool namespaceFound = false;
            string def = ref->getDefaultNamespace(), ns = "";
            map<string, string> namespaces = com->getNamespaces();
            map<string, string>::iterator it;
            for (it = namespaces.begin(); it != namespaces.end(); it++)
            {
                if (it->first == def)
                {
                    namespaceFound = true;
                    break;
                }
            }
            if (!namespaceFound)
            {
                namespaces = ref->getNamespaces();
                for (it = namespaces.begin(); it != namespaces.end(); it++)
                {
                    if (it->first == def)
                    {
                        ns = it->second;
                        topFile->getNamespace()->addNamespace(def, ns);

                        vector<sdfData*> dtypes = topFile->getDatatypes();
                        for (int i = 0; i < dtypes.size(); i++)
                        {
                            if (topFile->getInfo() && dtypes.at(i)->getName()
                                    == topFile->getInfo()->getTitle() + "-info")
                            {
                                addOriginNote(dtypes.at(i), "augmented_ns",
                                        def);
                            }
                        }
                        break;
                    }
                }
            }
        }
    }

    return stillLeft;
}

sdfFile* moduleToSdfFile(lys_module *module);

sdfObject* containerToSdfObject(lys_node_container *cont, sdfObject *object)
{
    if (!object)
        object = new sdfObject(avoidNull(cont->name),
                avoidNull(cont->dsc));
    else
    {
        object->setName(avoidNull(cont->name));
        object->setDescription(avoidNull(cont->dsc));
    }

    sdfData *buf = nodeToSdfData((lys_node*)cont, object);
    object->setDescription(buf->getDescription());

    vector<sdfData*> bufProps = buf->getObjectProperties();
    sdfProperty *p;
    for (int i = 0; i < bufProps.size(); i++)
    {
        p = new sdfProperty(*bufProps.at(i));
        object->addProperty(p);

        // change entry in referencesLeft to match new sdfProperty
        for (int j = 0; j < referencesLeft.size(); j++)
        {
            if (bufProps.at(i) == get<sdfCommon*>(referencesLeft.at(j)))
                get<sdfCommon*>(referencesLeft.at(j)) = p;
        }
    }

    return object;
}

void removeNode(lys_node &node);
void addNode(lys_node &child, lys_node &parent, lys_module &module);

sdfThing* containerToSdfThing(lys_node_container *cont, sdfThing *thing)
{
    if (!thing)
        thing = new sdfThing(avoidNull(cont->name),
                avoidNull(cont->dsc));
    else
    {
        thing->setName(avoidNull(cont->name));
        thing->setDescription(avoidNull(cont->dsc));
    }

    string origin;
    sdfThing *t;
    sdfObject *o;
    for (lys_node *elem = cont->child; elem; elem = elem->next)
    {
        origin = sdfSpecExtToString(elem->ext, elem->ext_size);
        if (elem->nodetype == LYS_CONTAINER)
        {
            lys_node_container *cont = (lys_node_container*)elem;

            if (origin == "sdfThing")
            {
                t = containerToSdfThing(cont, NULL);
                thing->addThing(t);
            }
            else
            {
                o = containerToSdfObject(cont, NULL);
                thing->addObject(o);
            }
        }
        else
            cerr << "containerToSdfThing: round tripped sdfThing should only "
                    "have container nodes and nothing else" << endl;
    }

    return thing;
}

sdfFile* moduleToSdfFile(lys_module *module)
{
    sdfFile *file = new sdfFile();

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
    file->setInfo(new sdfInfoBlock(avoidNull(module->name),
            avoidNull(module->rev->date), copyright, license));

    // Convert the namespace information
    map<string, string> nsMap;
    nsMap[avoidNull(module->prefix)] = avoidNull(module->ns);
    file->setNamespace(new sdfNamespaceSection(nsMap,
            avoidNull(module->prefix)));

    // add an extra sdfData for module info
    sdfData *mInfo = new sdfData();
    mInfo->setName(avoidNull(module->name) + "-info");
    mInfo->setDescription(avoidNull(module->dsc));

    if (module->ref)
        addOriginNote(mInfo, "reference", avoidNull(module->ref));

    for (int i = 0; i < module->rev_size; i++)
        addOriginNote(mInfo, "revision", avoidNull(module->rev[i].date));

    if (module->org)
        addOriginNote(mInfo, "organization", avoidNull(module->org));

    if (module->contact)
        addOriginNote(mInfo, "contact", avoidNull(module->contact));

    if (module->augment_size > 0)
        addOriginNote(mInfo, "augment_size", to_string(module->augment_size));

    for (int i = 0; i < module->features_size; i++)
        addOriginNote(mInfo, "feature", avoidNull(module->features[i].name));

    if (!mInfo->getDescription().empty())
        file->addDatatype(mInfo);

    // create buffer top-node to facilitate use of existing methods
    shared_ptr<lys_node_container> topNode (new lys_node_container());
    storeVoidPointer((shared_ptr<void>)topNode);
    topNode->name = storeString("buffer");
    topNode->dsc = storeString("buffer");
    topNode->nodetype = LYS_CONTAINER;
    topNode->child = module->data;
    topNode->module = module;
    topNode->parent = NULL;
    topNode->next = NULL;
    topNode->prev = (lys_node*)topNode.get();
    module->data = (lys_node*)topNode.get();
    for (lys_node *elem = topNode->child; elem; elem = elem->next)
        elem->parent = (lys_node*)topNode.get();

    // Translate typedefs of the module to sdfData of the sdfObject
    for (int i = 0; i < module->tpdf_size; i++)
        file->addDatatype(typedefToSdfData(&module->tpdf[i]));

    // Translate identities of the module to sdfData of the sdfObject
    sdfData *ident;
    for (int i = 0; i < module->ident_size; i++)
    {
        ident = identToSdfData(module->ident[i]);
        file->addDatatype(ident);
        identities[module->ident[i].name] = ident;
    }

    // Add identities and typedefs of the submodule to the sdfObject
    // What about the other members of submodule and further levels of
    // submodules?
    // -> The nodes of submodules are already added to the module
    // by libyang automatically
    for (int i = 0; i < module->inc_size; i++)
    {
        for (int j = 0; j < module->inc[i].submodule->tpdf_size; j++)
            file->addDatatype(typedefToSdfData(
                    &module->inc[i].submodule->tpdf[j]));

        sdfData *ident;
        for (int j = 0; j < module->inc[i].submodule->ident_size; j++)
        {
            ident = identToSdfData(module->inc[i].submodule->ident[j]);
            file->addDatatype(ident);
            identities[module->inc[i].submodule->ident[j].name] = ident;
        }
    }

    // Translate imported modules
    if (module->imp_size > 0)
        cout << "Converting imported YANG models to SDF..." << endl;
    for (int i = 0; i < module->imp_size; i++)
    {
        cout << "..." + avoidNull(module->imp[i].module->name) << endl;
        // check if the imported module has already been translated
        if (avoidNull(module->imp[i].module->name) != "sdf_extension"
                && find(alreadyImported.begin(), alreadyImported.end(),
                string(module->imp[i].module->name)) == alreadyImported.end())
        {
            sdfFile *importF = moduleToSdfFile(module->imp[i].module);
            importF->toFile(outputDirString
                    + avoidNull(module->imp[i].module->name) + ".sdf.json");

            alreadyImported.push_back(avoidNull(module->imp[i].module->name));
        }
        if (module->imp[i].module->prefix && module->imp[i].module->ns)
        {
            file->getNamespace()->addNamespace(
                    avoidNull(module->imp[i].module->prefix),
                    avoidNull(module->imp[i].module->ns));
        }
    }
    if (module->imp_size > 0)
        cout << "-> finished" << endl;

    // if the module is actually a submodule, return without iterating over
    // the child nodes (which do not exist in a submodule with type 1)
    if (module->type == 1)
        return file;

    // first mark containers that are to be converted to objects or things
    map<string, bool> alreadyAdded;
    vector<lys_node*> remove;
    for (lys_node *elem = topNode->child; elem; elem = elem->next)
    {
        if (elem->nodetype == LYS_CONTAINER)
        {
            alreadyAdded[avoidNull(elem->name)] = true;
            elem->flags |= IGNORE_NODE;
        }
        else
            alreadyAdded[avoidNull(elem->name)] = false;
    }

    // create a buffer sdfObject to be able to use existing methods
    sdfProperty *p;
    sdfObject *bufObject = new sdfObject();
    sdfData *buf = nodeToSdfData((lys_node*)topNode.get(), bufObject);

    vector<sdfData*> bufProps = buf->getObjectProperties();
    for (int i = 0; i < bufProps.size(); i++)
    {
        // only add if not already added (from container)
        if (!alreadyAdded[bufProps.at(i)->getName()])
        {
            p = new sdfProperty(*bufProps.at(i));
            file->addProperty(p);

            // change entry in referencesLeft to match new sdfProperty
            for (int j = 0; j < referencesLeft.size(); j++)
            {
                if (bufProps.at(i) == get<sdfCommon*>(referencesLeft.at(j)))
                    get<sdfCommon*>(referencesLeft.at(j)) = p;
            }
        }
    }

    vector<sdfAction*> bufActions = bufObject->getActions();
    for (int i = 0; i < bufActions.size(); i++)
        file->addAction(bufActions.at(i));

    vector<sdfEvent*> bufEvents = bufObject->getEvents();
    for (int i = 0; i < bufEvents.size(); i++)
        file->addEvent(bufEvents.at(i));

    vector<sdfData*> bufDatatypes = bufObject->getDatatypes();
    for (int i = 0; i < bufDatatypes.size(); i++)
        file->addDatatype(bufDatatypes.at(i));

    // add removed nodes back in in case they were referenced somewhere
//    for (int i = 0; i < remove.size(); i++)
//        addNode(*remove.at(i), (lys_node&)*topNode, *topNode->module);

    // convert top-level containers to objects (or things)
    string origin;
    sdfThing *t;
    sdfObject *o;
    for (lys_node *elem = topNode->child; elem; elem = elem->next)
    {
        origin = sdfSpecExtToString(elem->ext, elem->ext_size);
        if (elem->nodetype == LYS_CONTAINER)
        {
            elem->flags &= ~IGNORE_NODE;
            lys_node_container *cont = (lys_node_container*)elem;

            // this only happens in round trips
            if (origin == "sdfThing")
            {
                t = containerToSdfThing(cont, NULL);
                file->addThing(t);
            }
            else
            {
                o = containerToSdfObject(cont, NULL);
                file->addObject(o);
            }
        }
    }

    // check for references left unassigned
    map<string, sdfCommon*> existingConversions;
    existingConversions.insert(leafs.begin(), leafs.end());
    existingConversions.insert(branchRefs.begin(), branchRefs.end());
    referencesLeft = assignReferences(referencesLeft, existingConversions);
    typerefs = assignReferences(typerefs, typedefs);
    identsLeft = assignReferences(identsLeft, identities);

    if (!referencesLeft.empty() || !typerefs.empty() || !identsLeft.empty())
        cerr << file->getInfo()->getTitle() << ": "
        +  to_string(referencesLeft.size()+typerefs.size()+identsLeft.size())
        + " unresolved references remaining" << endl;

    return file;
}

lys_ext_instance** argToSdfSpecExtension(string arg, lys_ext_instance **exts,
                                            uint8_t exts_size)
{
    int i = exts_size;
    if (exts_size == 0)
    {
        shared_ptr<lys_ext_instance*[]> extsNew(new lys_ext_instance*[10]());
        exts = (lys_ext_instance**)storeVoidPointer((shared_ptr<void>)extsNew);
    }
    shared_ptr<lys_ext_instance> ext(new lys_ext_instance());

    exts[i] = (lys_ext_instance*)storeVoidPointer((shared_ptr<void>)ext);

    exts[i]->def = &helper->extensions[0];
    exts[i]->arg_value = storeString(arg);
    exts[i]->flags = 0;
    exts[i]->ext_size = 0;
    exts[i]->insubstmt_index = 0;
    exts[i]->insubstmt = 0;
    exts[i]->parent_type = 1;
    exts[i]->ext_type = LYEXT_FLAG;
    exts[i]->ext = NULL;
    exts[i]->nodetype = LYS_EXT;

    return exts;
}

void setSdfSpecExtension(lys_tpdf *tpdf, string arg)
{
    if (!tpdf->ext)
        tpdf->ext_size = 0;
    tpdf->ext = argToSdfSpecExtension(arg, tpdf->ext, tpdf->ext_size++);
    tpdf->ext[tpdf->ext_size-1]->parent = (void*)tpdf;
    tpdf->ext[tpdf->ext_size-1]->module = tpdf->module;
}
void setSdfSpecExtension(lys_node *node, string arg)
{
    if (!node->ext)
        node->ext_size = 0;
    node->ext = argToSdfSpecExtension(arg, node->ext, node->ext_size++);
    node->ext[node->ext_size-1]->parent = (void*)node;
    node->ext[node->ext_size-1]->module = node->module;

}

string removeConvNote(string dsc)
{
    // erase the conversion notes
    regex r("!Conversion note: ([^!]*)!\n");
    return regex_replace(dsc, r, "");
}

string removeConvNote(char *dsc)
{
    // erase the conversion notes
    regex r("!Conversion note: ([^!]*)!\n");
    return regex_replace(dsc, r, "");
}

vector<tuple<string, string>> extractConvNote(sdfCommon *com)
{
    string first = "", second = "", dsc = com->getDescription(), str;
    vector<tuple<string, string>> result;
    regex r("!Conversion note: ([^!]*)!\n");
    smatch sm;

    while (regex_search(dsc, sm, r))
    {
        str = sm[1].str();
        dsc = sm.suffix().str();
        first  = str.substr(0, str.find(" "));
        if (str.find(" ") != str.npos)
            second = str.erase(0, str.find(" ")+1);
        result.push_back(tuple<string, string> {first, second});
    }

    return result;
}

/*
 * RFC 7950 3. Terminology: A mandatory node is
 * a leaf/choice/anydata/anyxml node with "mandatory true",
 * a list/leaf-list node with "min-elements" > 0
 * or a non-presence container node with at least one mandatory child node
 */
bool setMandatory(lys_node *node, bool firstLevel)
{
    if (!node)
        return false;

    // Go further until ALL child nodes are mandatory? no

    // leaf/choice nodes can be assigned the mandatory flag directly
    if (node->nodetype & (LYS_LEAF | LYS_CHOICE))
    {
        node->flags |= LYS_MAND_TRUE;
        return true;
    }
    if (node->nodetype & (LYS_LEAFLIST | LYS_LIST))
    {
        if (((lys_node_list*)node)->min < 1)
            ((lys_node_list*)node)->min = 1;
        return true;
    }
    if (node->nodetype & LYS_CONTAINER)
        ((lys_node_container*)node)->presence = NULL;

    bool success = false;
    if (!firstLevel)
        success = setMandatory(node->next, false);

    if (!success)
        success = setMandatory(node->child, false);

    return success;
}

void sdfRequiredToNode(vector<sdfCommon*> reqs, lys_module &module)
{
    for (int i = 0; i < reqs.size(); i++)
    {
        lys_node *node = pathsToNodes[reqs[i]->generateReferenceString(true)];
        if (!node)
            cerr << "Node " + reqs[i]->getName() + " not found" << endl;


        setSdfSpecExtension(node, "sdfRequired");

        if (!setMandatory(node))
            cerr << "Conversion of sdfRequired "
            + reqs[i]->generateReferenceString() + " to 'mandatory' failed"
            << endl;
    }
}

void fillLysType(sdfData *data, struct lys_type &type,
        vector<tuple<sdfCommon*, lys_type*>> &openRefsType)
{
    if (!data)
    {
        cerr << "fillLysType: sdfData element must not be NULL" << endl;
        type.base = LY_TYPE_EMPTY;
        type.der = &emptyTpdf;
        return;
    }
    vector<tuple<string, string>> convNote = extractConvNote(data);
    string origType = "";
    vector<string> origPattern = {};
    vector<string> origPatternInvert = {};
    for (int i = 0; i < convNote.size(); i++)
    {
        if (get<0>(convNote[i]) == "type")
            origType = get<1>(convNote[i]); // require-instance?
        if (get<0>(convNote[i]) == "pattern")
            origPattern.push_back(get<1>(convNote[i]));
        if (get<0>(convNote[i]) == "pattern-invert-match")
            origPatternInvert.push_back(get<1>(convNote[i]));
    }
    if (origType != "")
        type.base = stringToLType(origType);
    else
        type.base = stringToLType(data->getSimpType());

    // if no type is given
    if (type.base == LY_TYPE_UNKNOWN)
    {
        if (data->getDefaultIntDefined() || data->getConstantIntDefined())
            type.base = stringToLType(json_integer);

        else if (data->getDefaultBoolDefined() || data->getConstantBoolDefined())
            data->setSimpType(json_boolean);

        else if (!isnan(data->getMinimum()) || !isnan(data->getMaximum())
                || !isnan(data->getExclusiveMinimumNumber())
                || !isnan(data->getExclusiveMaximumNumber())
                || !isnan(data->getMultipleOf())
                || !isnan(data->getConstantNumber())
                || !isnan(data->getDefaultNumber()))
            type.base = stringToLType(json_number);

        else if (!isnan(data->getMinLength())
                || !isnan(data->getMaxLength()) || data->getPattern() != ""
                || !data->getEnumString().empty()
                || data->getDefaultString() != ""
                || data->getConstantString() != "")
            type.base = stringToLType(json_string);

        else if (!isnan(data->getMinItems()) || !isnan(data->getMaxItems())
                || data->getItemConstr() || data->getUniqueItemsDefined()
                || !data->getDefaultBoolArray().empty()
                || !data->getDefaultIntArray().empty()
                || !data->getDefaultStringArray().empty()
                || !data->getDefaultNumberArray().empty()
                || !data->getConstantBoolArray().empty()
                || !data->getConstantIntArray().empty()
                || !data->getConstantStringArray().empty()
                || !data->getConstantNumberArray().empty())
        {
            type.base = stringToLType(json_array);
            shared_ptr<lys_tpdf> der(new lys_tpdf());
            voidPointerStore.push_back((shared_ptr<void>)der);
            type.der = der.get();
        }

        else if (!data->getRequiredObjectProperties().empty()
                || !data->getObjectProperties().empty())
            type.base = stringToLType(json_object);
        else
        {
            type.base = LY_TYPE_EMPTY;
            type.der = &emptyTpdf;
            // leave it out for now to not miss mistakes

            // until then, do the following
//            shared_ptr<lys_tpdf> der(new lys_tpdf());
//            voidPointerStore.push_back((shared_ptr<void>)der);
//            type.der = der.get();
        }
    }

    if (!data->getEnumString().empty())
    {
        type.base = LY_TYPE_ENUM;
        type.der = &enumTpdf;
        int enmSize = data->getEnumString().size();
        vector<string> enm = data->getEnumString();
        type.info.enums.count = enmSize;
        shared_ptr<lys_type_enum[]> e(new lys_type_enum[enmSize]());
        storeVoidPointer((shared_ptr<void>)e);
        type.info.enums.enm = e.get();
        for (int i = 0; i < enmSize; i++)
        {
            type.info.enums.enm[i].name = storeString(enm[i]);
            type.info.enums.enm[i].value = i;
        }
    }

    else if (type.base == LY_TYPE_BOOL)
    {
        type.der = &booleanTpdf;
    }

    else if (type.base == LY_TYPE_STRING || type.base == LY_TYPE_BINARY
            || data->getSubtype() == sdf_byte_string)
    {
        if (data->getSubtype() == sdf_byte_string)
            type.base == LY_TYPE_BINARY;

        if (type.base == LY_TYPE_STRING)
            type.der = &stringTpdf;
        else if (type.base == LY_TYPE_BINARY)
            type.der = &binaryTpdf;

        const char *range = floatToRange(data->getMinLength(),
                data->getMaxLength());
        if (range)
        {
            lys_restr lenRestr = {};
            lenRestr.expr = range;

            if (type.base == LY_TYPE_STRING)
                type.info.str.length = storeRestriction(lenRestr);
            else if (type.base == LY_TYPE_BINARY)
                type.info.binary.length = storeRestriction(lenRestr);
        }
        char ack = 0x06; // ACK for match
        char nack = 0x15; // NACK for invert match
        unsigned int patCnt = 0;
        shared_ptr<lys_restr[]> patterns(new lys_restr[2]());
        voidPointerStore.push_back(patterns);
        type.info.str.patterns = patterns.get();//new lys_restr[2]();
        if (data->getConstantString() != "")
        {
            lys_restr constRestr = {};
            constRestr.expr = storeString(ack + data->getConstantString());
            type.info.str.patterns[patCnt++] = *storeRestriction(constRestr);
        }
        if (data->getPattern() != "")
        {
            lys_restr patRestr = {};

            string pat = data->getPattern();
            smatch sm;
            regex rNot("\\(\\(\\?!([^\\)])\\)\\.\\)");

            // transfer invert match patterns
            int oldSize = origPattern.size();
            for (int i = 0; i < origPatternInvert.size(); i++)
                origPattern.push_back(nack + origPatternInvert.at(i));
            for (int i= 0; i < origPattern.size(); i++)
            {
                if (i < oldSize)
                    origPattern.at(i) = ack + origPattern.at(i);
                patRestr = {};
                patRestr.expr = storeString(origPattern.at(i));
                type.info.str.patterns[patCnt++] = *storeRestriction(patRestr);
            }

            if (origPattern.empty())
            {
                if (regex_match(pat, sm, rNot))
                    pat = nack + sm[1].str();
                else
                    pat = ack + pat;

                patRestr = {};
                patRestr.expr = storeString(pat);
                type.info.str.patterns[patCnt++] = *storeRestriction(patRestr);
            }

        }
        type.info.str.pat_count = patCnt;
    }

    else if (type.base == LY_TYPE_DEC64 || type.base == LY_TYPE_INT64
            || type.base == LY_TYPE_INT32 || type.base == LY_TYPE_INT16
            || type.base == LY_TYPE_INT8 || type.base == LY_TYPE_UINT64
            || type.base == LY_TYPE_UINT32 || type.base == LY_TYPE_UINT16
            || type.base == LY_TYPE_UINT8)
    {
        const char *constRange = storeString(data->getConstantAsString());

        // if min or max are the limits or their type remove them
        int64_t min = data->getMinInt();
        uint64_t max = data->getMaxInt();

        double minN = data->getMinimum();
        double maxN = data->getMaximum();

        // check for exclusiveMinimum and exclusiveMaximum
        if(data->isExclusiveMinimumBool())
            min++;
        if(data->isExclusiveMaximumBool())
            max--;

        // check for exclusiveMinimum and exclusiveMaximum
        if(!isnan(data->getExclusiveMinimumNumber()))
        {
            min = (int)data->getExclusiveMinimumNumber() + 1;
            minN = data->getExclusiveMinimumNumber();
            if (data->getMultipleOf())
                minN += data->getMultipleOf();
        }
        if(!isnan(data->getExclusiveMaximumNumber()))
        {
            max = (int)data->getExclusiveMaximumNumber() - 1;
            cout << max << endl;
            maxN = data->getExclusiveMaximumNumber();
            if (data->getMultipleOf())
                maxN -= data->getMultipleOf();
        }

        const char *range;
        if (data->getMinIntSet() && data->getMaxIntSet())
            range = intToRange(min, max);
        else if (data->getMinIntSet())
            range = storeString(to_string(min));
        else if (data->getMaxIntSet())
            range = storeString(to_string(max));
        else
            range = floatToRange(minN, maxN);

        // pre-determine the derived type reference
        lys_tpdf *der;
        switch (type.base)
        {
            case LY_TYPE_INT64:
                der = &intTpdf;
                break;
            case LY_TYPE_UINT64:
                der = &uint64Tpdf;
                break;
            case LY_TYPE_INT32:
                der = &int32Tpdf;
                break;
            case LY_TYPE_UINT32:
                der = &uint32Tpdf;
                break;
            case LY_TYPE_INT16:
                der = &int16Tpdf;
                break;
            case LY_TYPE_UINT16:
                der = &uint16Tpdf;
                break;
            case LY_TYPE_INT8:
                der = &int8Tpdf;
                break;
            case LY_TYPE_UINT8:
                der = &uint8Tpdf;
                break;
            case LY_TYPE_DEC64:
                der = &dec64Tpdf;
                break;
        }

        // if either a constant xor a min-max range is given
        // put the num/dec64 range to whatever is given
        if ((constRange && !range) || (!constRange && range))
        {
            lys_restr rangeRestr = {};
            if (constRange)
                rangeRestr.expr = constRange;
            else if (range)
                rangeRestr.expr = range;

            if (type.base == LY_TYPE_INT64
                    || type.base == LY_TYPE_INT32
                    || type.base == LY_TYPE_INT16
                    || type.base == LY_TYPE_INT8
                    || type.base == LY_TYPE_UINT64
                    || type.base == LY_TYPE_UINT32
                    || type.base == LY_TYPE_UINT16
                    || type.base == LY_TYPE_UINT8)
            {
                type.der = der;
                type.info.num.range = storeRestriction(rangeRestr);
            }
            else
            {
                type.der = &dec64Tpdf;
                type.info.dec64.range = storeRestriction(rangeRestr);

                // number of fraction-digits: std::to_string is always 6
                if (isnan(data->getMultipleOf()))
                    type.info.dec64.dig = 6;
                else
                {
                    double mO = data->getMultipleOf();
                    for (int j = 0; mO < 10; j++)
                    {
                        mO = mO * 10;
                        type.info.dec64.dig = j;
                    }
                }
            }
        }
        // if both a constant and a min-max range are given, create a union
        else if (constRange && range)
        {
            lys_restr constRestr = {};
            lys_restr rangeRestr = {};
            constRestr.expr = constRange;
            rangeRestr.expr = range;

            type.info.uni.count = 2;
            shared_ptr<lys_type[]> types(new lys_type[2]());
            voidPointerStore.push_back(types);
            type.info.uni.types = types.get();

            if (type.base == LY_TYPE_INT64
                    || type.base == LY_TYPE_INT32
                    || type.base == LY_TYPE_INT16
                    || type.base == LY_TYPE_INT8
                    || type.base == LY_TYPE_UINT64
                    || type.base == LY_TYPE_UINT32
                    || type.base == LY_TYPE_UINT16
                    || type.base == LY_TYPE_UINT8)
            {
                lys_type t =
                {
                        .base = type.base,
                        .der = der,
                        .parent = type.parent,
                };
                t.info.num.range =
                        storeRestriction(constRestr);
                type.info.uni.types[0] = t;
                t.info.num.range =
                        storeRestriction(rangeRestr);
                type.info.uni.types[1] = t;
            }
            else
            {
                lys_type t =
                {
                        .base = LY_TYPE_DEC64,
                        .der = &dec64Tpdf,
                        .parent = type.parent,
                };
                if (isnan(data->getMultipleOf()))
                    t.info.dec64.dig = 6;
                else
                {
                    double mO = data->getMultipleOf();
                    for (int j = 0; mO < 10; j++)
                    {
                        mO = mO * 10;
                        t.info.dec64.dig = j;
                    }
                }

                t.info.dec64.range =
                        storeRestriction(constRestr);
                type.info.uni.types[0] = t;
                t.info.dec64.range =
                        storeRestriction(rangeRestr);
                type.info.uni.types[1] = t;
                type.info.uni.count = 2;
            }

            type.base = LY_TYPE_UNION;
            type.der = &unionTpdf;
        }
        else
        {
            if (type.base == LY_TYPE_INT64
                    || type.base == LY_TYPE_INT32
                    || type.base == LY_TYPE_INT16
                    || type.base == LY_TYPE_INT8
                    || type.base == LY_TYPE_UINT64
                    || type.base == LY_TYPE_UINT32
                    || type.base == LY_TYPE_UINT16
                    || type.base == LY_TYPE_UINT8)
            {
                type.der = der;
            }
            else
            {
                type.der = &dec64Tpdf;
                if (isnan(data->getMultipleOf()))
                    type.info.dec64.dig = 6;
                else
                {
                    double mO = data->getMultipleOf();
                    for (int j = 0; mO < 10; j++)
                    {
                        mO = mO * 10;
                        type.info.dec64.dig = j;
                    }
                }
            }
        }
    }
    else if (type.base == LY_TYPE_IDENT)
    {
        type.der = &identTpdf;
        vector<sdfData*> op = data->getObjectProperties();
        sdfCommon *ref = data->getReference();
        shared_ptr<lys_ident*[]> refs(new lys_ident*[ref? 1 : op.size()]());
        if (!op.empty() || ref )
        {
            storeVoidPointer((shared_ptr<void>)refs);
            type.info.ident.ref = refs.get();
        }
        type.info.ident.count = 0;

        for (int i = 0; i < op.size(); i++)
        {
            ref = op[i]->getReference();
            if (ref)
            {
                openBaseIdent.push_back(tuple<string, lys_ident**>{
                    ref->generateReferenceString(true),
                            &type.info.ident.ref[i]});
                type.info.ident.count++;
            }
            else
                cerr << "convertDatatypes: origin identity with multiple "
                        "bases but one sdfRef is null" << endl;
        }
        ref = data->getReference();
        if (ref)
        {
            openBaseIdent.push_back(tuple<string, lys_ident**>{
                ref->generateReferenceString(true), &type.info.ident.ref[0]});
            type.info.ident.count = 1;
        }

    }
    else if (type.base == LY_TYPE_EMPTY)
    {
        type.der = &emptyTpdf;
    }
    else if (type.base == LY_TYPE_BITS)
    {
        type.der = &bitsTpdf;

        vector<sdfData*> op = data->getObjectProperties();
        shared_ptr<lys_type_bit[]> bits(new lys_type_bit[op.size()]());
        storeVoidPointer((shared_ptr<void>)bits);
        type.info.bits.bit = bits.get();
        type.info.bits.count = op.size();
        regex pos("Bit at position ([0-9]+):");
        smatch sm;
        string dsc;
        for (int i = 0; i < op.size(); i++)
        {
            type.info.bits.bit[i].name = storeString(op[i]->getName());
            dsc = op[i]->getDescription();
            type.info.bits.bit[i].dsc = storeString(dsc);

            if (regex_search(dsc, sm, pos))
                type.info.bits.bit[i].pos = stoi(sm[1].str());
        }
        if (op.empty())
            cerr << "fillLysType: type bits but no object properties" << endl;
    }

    else if (type.base == LY_TYPE_UNION)
    {
        type.der = &unionTpdf;
        vector<sdfData*> choices = data->getChoice();

        type.info.uni.count = choices.size();
        shared_ptr<lys_type[]> t(new lys_type[type.info.uni.count]());
        storeVoidPointer((shared_ptr<void>)t);
        type.info.uni.types = t.get();

        for (int i = 0; i < choices.size(); i++)
        {
            type.info.uni.types[i].parent = type.parent;
            fillLysType(choices[i], type.info.uni.types[i], openRefsType);
            if (choices[i]->getReference())
            {
                openRefsType.push_back(tuple<sdfCommon*, lys_type*>{
                    choices[i], &type.info.uni.types[i]});
            }

            if (type.info.uni.types[i].base & (LY_TYPE_LEAFREF | LY_TYPE_INST))
                type.info.uni.has_ptr_type = 1;
        }
        // do this to prevent another choice conversion later (?)
        data->setChoice({});
    }

    for (int i = 0; i < data->getChoice().size(); i++)
    {

    }
}

void addNode(lys_node &child, lys_node &parent, lys_module &module)
{
    child.parent = &parent;
    child.module = &module;

    if (!parent.child)
    {
        parent.child = &child;
        child.prev = &child;
    }
    else
    {
        lys_node *sibling = parent.child;
        while (sibling->next)
            sibling = sibling->next;
        sibling->next = &child;
        child.prev = sibling;
    }
}

void addNode(lys_node &child, lys_module &module)
{
    child.parent = NULL;
    child.module = &module;

    if (!module.data)
    {
        module.data = &child;
        child.prev = &child;
    }
    else
    {
        lys_node *sibling = module.data;
        while (sibling->next)
            sibling = sibling->next;
        sibling->next = &child;
        child.prev = sibling;
    }
}

void removeNode(lys_node &node)
{
    // if the node is not a top-level node
    if (node.parent)
    {
        // if the node is the first of its siblings
        if (node.parent->child == &node)
        {
            // if the node is an only child
            if (!node.next)
                node.parent->child = NULL;

            else
                node.parent->child = node.next;
        }
        node.parent = NULL;
    }

    // if the node is a top-level node
    else if (node.module)
    {
        // if the node is the first of its siblings
        if (node.module->data == &node)
        {
            // if the node is an only child
            if (!node.next)
                node.module->data = NULL;
            else
                node.module->data = node.next;
        }
        node.module = NULL;
    }

    if (node.prev != &node && node.prev)
    {
        node.prev->next = node.next;
        if (node.next)
            node.next->prev = node.prev;
    }
    else if (node.next)
        node.next->prev = node.next;

    node.prev = NULL;
    node.next = NULL;
    node.parent = NULL;
    node.module = NULL;
}

lys_type* sdfRefToType(string refString, lys_type *type)
{
    // Go through the tpdfStore and look for the tpdf corresponding to the
    // element referenced by refString
    for (int i = 0; i < tpdfStore.size(); i++)
    {
        if (pathsToNodes[refString] == (lys_node*)tpdfStore[i])
        {
//            type->base = LY_TYPE_DER;
            type->der = tpdfStore[i];
            return type;
        }
    }
    cerr << "sdfRefToType: typedef corresponding to sdfRef " + refString
            + " not found" << endl;
    return NULL;
}

lys_node_leaf* findLeafInSubtreeRecursive(lys_node *node)
{
    for (lys_node *child = node->child; child; child = child->next)
    {
        if (child->nodetype == LYS_LEAF)
            return (lys_node_leaf*)child;
        else
            return findLeafInSubtreeRecursive(child);
    }
    return NULL;
}

lys_module* sdfFileToModule(sdfFile &file, lys_module &module,
        vector<tuple<sdfCommon*, lys_node*>> &openRefs,
        vector<tuple<sdfCommon*, lys_tpdf*>> &openRefsTpdf,
        vector<tuple<sdfCommon*, lys_type*>> &openRefsType);

lys_node* sdfRefToNode(sdfCommon *com, lys_node *node, lys_module &module,
        bool nodeIsTpdf)
{
    if (!com || !com->getReference())
        // || (node->parent && node->parent->nodetype == LYS_AUGMENT && node->parent->module != &module))
        return NULL;
    if (!node)
    {
        cerr << "sdfRefToNode: node must not be NULL" << endl;
        return node;
    }
    if ((!nodeIsTpdf && !node->module)
        || (nodeIsTpdf && !((lys_tpdf*)node)->module))
    {
        cerr << "sdfRefToNode: node " + com->getName()
                + " has no module" << endl;
    }

    sdfCommon *ref = com->getReference();
    sdfData *refData = ref->getThisAsSdfData();
    string refString = ref->generateReferenceString(true);
    int size = nodeStore.size();

    // Go through the nodeStore and look for the node corresponding to the
    // element referenced by sdfRef (ref)
    for (int i = 0; i < size; i++)
    {
        if (ref && nodeStore[i]
                && pathsToNodes[refString] == nodeStore[i].get())
        {
            // find out if the node is part of a grouping
            bool isInGrp = false;
            for (lys_node *j = nodeStore[i]->parent; j; j = j->parent)
            {
                if (j->nodetype == LYS_GROUPING)
                {
                    isInGrp = true;
                    break;
                }
            }

            // if the node is part of a grouping it cannot be referenced
            // by a leafref
            if (!isInGrp && (nodeStore[i]->nodetype == LYS_LEAF
                    || nodeStore[i]->nodetype == LYS_LEAFLIST))
            {
                // leafref
                lys_type type = {};
                type.parent = ((lys_node_leaf*)node)->type.parent;
                type.base = LY_TYPE_LEAFREF;
                type.der = &leafrefTpdf;

                type.info.lref.target = (lys_node_leaf*)nodeStore[i].get();
                bool genPrefix = nodeStore[i]->module == &module ? false : true;
                type.info.lref.path = storeString(
                        generatePath(nodeStore[i].get(), &module, genPrefix));

                if (nodeIsTpdf)
                    ((lys_tpdf*)node)->type = type;
                if (node->nodetype == LYS_LEAF)
                    ((lys_node_leaf*)node)->type = type;
                else if (node->nodetype == LYS_LEAFLIST)
                    ((lys_node_leaflist*)node)->type = type;

                // leafrefs to non-config leafs must have config false
                if ((nodeStore[i]->flags & LYS_CONFIG_R) &&
                        (nodeStore[i]->flags & LYS_CONFIG_SET))
                {
                    node->flags |= LYS_CONFIG_R;
                    node->flags |= LYS_CONFIG_SET;
                }
            }
            else if (nodeStore[i]->nodetype == LYS_CONTAINER)
            {
                // change container to grouping and use that grouping with uses
                shared_ptr<lys_node_uses> uses =
                        shared_ptr<lys_node_uses>(new lys_node_uses());
                uses->nodetype = LYS_USES;

                shared_ptr<lys_node_grp> grp(new lys_node_grp());
                shared_ptr<lys_node_uses> uses2;
                shared_ptr<lys_node> refNode;

                grp = shared_ptr<lys_node_grp>(new lys_node_grp(
                        (lys_node_grp&)*nodeStore[i].get()));
                // remove former sibling node etc.
                removeNode((lys_node&)*grp);
                grp->nodetype = LYS_GROUPING;
                addNode((lys_node&)*grp, module);

                addNode((lys_node&)*uses, *node, module);
                uses->grp = grp.get();
                uses->name = uses->grp->name;

                // Replace the container that the grouping was copied from
                // by a uses to that grouping
                uses2 = shared_ptr<lys_node_uses>(new lys_node_uses(
                        *uses.get()));
                nodeStore[i]->child = NULL;
                addNode((lys_node&)*uses2, *nodeStore[i],
                        *nodeStore[i]->module);
                storeNode((shared_ptr<lys_node>&)grp);
                storeNode((shared_ptr<lys_node>&)uses);
                storeNode((shared_ptr<lys_node>&)uses2);
                storeNode(refNode);
            }
            else if (nodeStore[i]->nodetype == LYS_LIST
                    || (isInGrp))
            {
                shared_ptr<lys_node_uses> uses =
                        shared_ptr<lys_node_uses>(new lys_node_uses());
                uses->nodetype = LYS_USES;
                uses->module = &module;

                shared_ptr<lys_node_grp> grp(new lys_node_grp());

                // keep in case this should be done differently again
                /*lys_node *child = nodeStore[i]->child;
                addNode(*child, (lys_node&)*grp, module);
                while (child->next)
                {
                    child = child->next;
                    child->parent = (lys_node*)grp.get();
                }
                nodeStore[i]->child = NULL;*/

                // node has to be added at top-level due to scoping
                addNode((lys_node&)*grp, *nodeStore[i]->module);
                lys_node *origParent = nodeStore[i]->parent;
                removeNode(*nodeStore[i]);
                addNode(*nodeStore[i], (lys_node&)*grp, module);

                grp->name = nodeStore[i]->name;
                grp->nodetype = LYS_GROUPING;
                uses->grp = grp.get();
                string usesName;
                if (uses->grp->module != &module)
                    usesName = avoidNull(uses->grp->module->prefix) + ":"
                    + avoidNull(uses->grp->name);
                else
                    usesName = avoidNull(uses->grp->name);
                uses->name = storeString(usesName);//uses->grp->name;

                // lists must have config false statement otherwise they need
                // a key
                nodeStore[i]->flags |= LYS_CONFIG_R;
                nodeStore[i]->flags |= LYS_CONFIG_SET;

                //addNode((lys_node&)*uses, *node, module);
                if (node->parent)
                    addNode(*storeNode((shared_ptr<lys_node>&)uses),
                            *node->parent, module);
                else if (node->module)
                    addNode(*storeNode((shared_ptr<lys_node>&)uses),
                            *node->module);
                else
                    cerr << "sdfRefToNode: node " + avoidNull(node->name)
                    + " must have a module" << endl;

                // Replace the node that the grouping was copied from
                // by a uses to that grouping
                shared_ptr<lys_node_uses> uses2(new lys_node_uses(
                        *uses.get()));
                uses2->name = uses2->grp->name;
                //addNode((lys_node&)*uses2, *grp->parent, module);
                if (origParent)
                    addNode(*storeNode((shared_ptr<lys_node>&)uses2),
                            *origParent, *origParent->module);
                else if (nodeStore[i]->module)
                    addNode(*storeNode((shared_ptr<lys_node>&)uses2),
                            *nodeStore[i]->module);
                else
                    cerr << "sdfRefToNode: node " + avoidNull(node->name)
                    + " must have a module" << endl;

                // augment for additional object properties? is that
                // even possible? no
                shared_ptr<lys_refine> refine(new lys_refine());
                voidPointerStore.push_back((shared_ptr<void>)refine);
                uses->refine = refine.get();
                //uses->refine = new lys_refine();
                uses->refine->module = &module;
                uses->refine->target_name = grp->child->name;
                uses->refine->target_type = grp->child->nodetype;
                string dsc = avoidNull(node->name) + "\n"
                        + avoidNull(node->dsc);
                uses->refine->dsc = storeString(dsc);
                uint32_t min = 0, max = 0;
                lys_node_list *l;
                if (node->nodetype == LYS_LIST)
                {
                    l = (lys_node_list*)node;
                    min = l->min;
                    max = l->max;

                    lys_node_leaf *keyLeaf = NULL;
                    if (l->keys_size == 0)
                    {
                        keyLeaf = findLeafInSubtreeRecursive(
                                (lys_node*)uses->grp);
                        if (keyLeaf)
                        {
                            l->keys[l->keys_size++] = keyLeaf;
                            l->keys_str = keyLeaf->name;
                            setSdfSpecExtension((lys_node*)keyLeaf,
                                    "artificial-key");
                        }
                    }
                }
                else if (node->nodetype == LYS_LEAFLIST)
                {
                    min = ((lys_node_leaflist*)node)->min;
                    max = ((lys_node_leaflist*)node)->max;
                }
                uses->refine->mod.list.min = min;
                uses->refine->mod.list.max = max;
                if (uses->refine->mod.list.min != 0)
                {
                    uses->refine->flags |= LYS_RFN_MINSET;
                    uses->refine_size = 1;
                }
                if (uses->refine->mod.list.max != 0)
                {
                    uses->refine->flags |= LYS_RFN_MAXSET;
                    uses->refine_size = 1;
                }

                removeNode(*node);

                pathsToNodes[refString] =
                        (lys_node*)grp.get();
                storeNode((shared_ptr<lys_node>&)grp);
                storeNode((shared_ptr<lys_node>&)uses2);
                storeNode((shared_ptr<lys_node>&)uses);
            }
            else if (nodeStore[i]->nodetype == LYS_GROUPING)
            {
                // uses
                shared_ptr<lys_node_uses> uses =
                        shared_ptr<lys_node_uses>(new lys_node_uses());
                uses->nodetype = LYS_USES;
                uses->grp = (lys_node_grp*)nodeStore[i].get();
                uses->module = &module;
                string usesName;
                if (uses->grp->module != &module)
                    usesName = avoidNull(uses->grp->module->prefix) + ":"
                    + avoidNull(uses->grp->name);
                else
                    usesName = avoidNull(uses->grp->name);
                uses->name = storeString(usesName);//uses->grp->name;

                // refine
                shared_ptr<lys_refine> refine(new lys_refine());
                voidPointerStore.push_back((shared_ptr<void>)refine);
                uses->refine = refine.get();
                uses->refine->module = &module;
                if (uses->grp->child)
                {
                    uses->refine->target_name = uses->grp->child->name;
                    uses->refine->target_type = uses->grp->child->nodetype;
                }

                // check if the used grouping contains a list or a using to a
                // grouping that contains a list and so on
                bool isList = false;
                lys_node *n = (lys_node*)uses->grp->child;
                if (!n)
                    cerr << "sdfRefToNode: uses " + avoidNull(uses->name)
                    + " has an empty grouping" << endl;
                // if the grouping has only one uses node
                while (n && n->nodetype == LYS_USES && !n->next)
                {
                    // if the uses has only one child of type list/leaflist/leaf
                    if (n->child && (n->child->nodetype &
                            (LYS_LIST | LYS_LEAF | LYS_LEAFLIST))
                            && !n->child->next)
                    {
                        isList = true;
                    }
                    n = ((lys_node_uses*)n)->grp->child;
                }
                if (n && (n->nodetype & (LYS_LIST | LYS_LEAF | LYS_LEAFLIST)))
                    isList = true;

                if (isList)
                {
                    uint32_t min = 0, max = 0;
                    lys_node_list *l;
                    if (node->nodetype == LYS_LIST)
                    {
                        l = (lys_node_list*)node;
                        min = l->min;
                        max = l->max;

                        lys_node_leaf *keyLeaf = NULL;
                        if (l->keys_size == 0)
                        {
                            keyLeaf = findLeafInSubtreeRecursive(
                                    (lys_node*)uses->grp);
                            if (keyLeaf)
                            {
                                l->keys[l->keys_size++] = keyLeaf;
                                l->keys_str = keyLeaf->name;
                                setSdfSpecExtension((lys_node*)keyLeaf,
                                        "artificial-key");
                            }
                        }
                    }
                    else if (node->nodetype == LYS_LEAFLIST)
                    {
                        min = ((lys_node_leaflist*)node)->min;
                        max = ((lys_node_leaflist*)node)->max;
                    }
                    uses->refine->mod.list.min = min;
                    uses->refine->mod.list.max = max;
                    if (uses->refine->mod.list.min != 0)
                    {
                        uses->refine->flags |= LYS_RFN_MINSET;
                        uses->refine_size = 1;
                    }
                    if (uses->refine->mod.list.max != 0)
                    {
                        uses->refine->flags |= LYS_RFN_MAXSET;
                        uses->refine_size = 1;
                    }
                }
                if (node->nodetype & (LYS_LEAF | LYS_LEAFLIST))
                    node->nodetype = LYS_CONTAINER;
                addNode(*storeNode((shared_ptr<lys_node>&)uses), *node,
                        module);
            }
            return node;
        }
    }

    // Go through the tpdfStore and look for the node corresponding to the
    // element referenced by sdfRef (ref)
    for (int i = 0; i < tpdfStore.size(); i++)
    {
        if (ref && pathsToNodes[refString]
                                == (lys_node*)tpdfStore[i])
        {
            lys_type *type;
            // type is stored differently in nodes and tpdfs so it is necessary
            // to make a distinction
            if (nodeIsTpdf)
                type = &((lys_tpdf*)node)->type;
            else
                type = &((lys_node_leaf*)node)->type;

            //type->base = LY_TYPE_DER;
            type->der = tpdfStore[i];

            return node;
        }
    }

    // if no match was found return NULL
    cerr << "No match found for reference " + refString + " of "
            + com->getName() << endl;
    return NULL;
}

struct lys_tpdf* sdfDataToTypedef(sdfData *data, lys_tpdf *tpdf,
        lys_module &module, vector<tuple<sdfCommon*, lys_tpdf*>> &openRefsTpdf,
        vector<tuple<sdfCommon*, lys_type*>> &openRefsType)
{
    if (!data)
    {
        cerr << "sdfDataToTypedef: sdfData element must not be NULL" << endl;
        return NULL;
    }
    tpdf->name = storeString(data->getName());
    string dsc = removeConvNote(data->getDescription());
    tpdf->dsc = storeString(dsc);
    tpdf->ref = NULL;
    tpdf->ext_size = 0;
    tpdf->module = &module;
    if (data->getUnits() != "")
        tpdf->units = storeString(data->getUnits());

    // type
    tpdf->type.parent = tpdf;
    fillLysType(data, tpdf->type, openRefsType);

    tpdf->dflt = storeString(data->getDefaultAsString());

    if (data->getReference()
            && avoidNull(tpdf->type.der->name) != "identityref")
    {
        openRefsTpdf.push_back(
             tuple<sdfCommon*, lys_tpdf*>{data, tpdf});
    }

    pathsToNodes[data->generateReferenceString(NULL, true)] =
            (lys_node*)tpdf;
    setSdfSpecExtension(tpdf, "sdfData");

    return tpdf;
}

lys_node* sdfDataToNode(sdfData *data, lys_node *node, lys_module &module,
        vector<tuple<sdfCommon*, lys_node*>> &openRefs,
        vector<tuple<sdfCommon*, lys_type*>> &openRefsType)
{
    if (!data)
    {
        cerr << "sdfDataToNode: sdfData element must not be NULL" << endl;
        return NULL;
    }

    sdfData *itemConstrWithRefs = data->getItemConstrOfRefs();
    sdfData *ref = data->getSdfDataReference();

    vector<tuple<string, string>> origV = extractConvNote(data);
    string origType = "", origStatus = "", origRef = "", origPresence = "",
            augByName = "", origConfig = "", orderedBy = "";
    bool origUnique = false;
    vector<string> keys;
    for (int i = 0; i < origV.size(); i++)
    {
        if (get<0>(origV[i]) == "type")
            origType = get<1>(origV[i]);
        else if (get<0>(origV[i]) == "status")
            origStatus = get<1>(origV[i]);
        else if (get<0>(origV[i]) == "reference")
            origRef = get<1>(origV[i]);
        else if (get<0>(origV[i]) == "presence")
            origPresence = get<1>(origV[i]);
        else if (get<0>(origV[i]) == "unique")
            origUnique = true;
        else if (get<0>(origV[i]) == "key")
            keys.push_back(get<1>(origV[i]));
        else if (get<0>(origV[i]) == "augment-by")
            augByName = get<1>(origV[i]);
        else if (get<0>(origV[i]) == "config")
            origConfig = get<1>(origV[i]);
        else if (get<0>(origV[i]) == "ordered-by")
            orderedBy = get<1>(origV[i]);
    }

    // type object -> container
    if (data->getSimpType() == json_object && origType != "bits"
            && origType != "empty")
    {
        shared_ptr<lys_node_container> cont(new lys_node_container());
        //lys_node_container *cont = new lys_node_container();
        cont->nodetype = LYS_CONTAINER;
        cont->presence = storeString(origPresence);
        lys_node *childNode;
        vector<sdfData*> properties = data->getObjectProperties();
        vector<string> req = data->getRequiredObjectProperties();
        vector<sdfCommon*> reqComs = {};
        for (int i = 0; i < properties.size(); i++)
        {
            childNode = sdfDataToNode(properties[i], childNode, module,
                    openRefs, openRefsType);
            // add the current node into the tree
            if (childNode)
                addNode(*childNode, (lys_node&)*cont, module);

            for (int j = 0; j < req.size(); j++)
            {
                if (req[j] == properties[i]->getName())
                    reqComs.push_back(properties[i]);
            }
        }
        sdfRequiredToNode(reqComs, module);
        node = storeNode((shared_ptr<lys_node>&)cont);
    }

    // type array with objects of type object -> list
    else if (data->getSimpType() == json_array
                && itemConstrWithRefs
                && itemConstrWithRefs->getSimpType() == json_object)
    {
        shared_ptr<lys_node_list> list =
                shared_ptr<lys_node_list>(new lys_node_list());
        list->nodetype = LYS_LIST;
        // do this? this is only done because of the way
        // sdfRefs to arrays with object items are translated into lists
        list->min = data->getMinItemsOfRef();
        list->max = data->getMaxItemsOfRef();

        // lists must have config false statement otherwise they need a key
        // -> just give it a key then
        //list->flags &= LYS_CONFIG_R;
        //list->flags += LYS_CONFIG_SET;

        lys_node *childNode;
        if (data->getItemConstr())
        {
            string keyNames = "";
            if (keys.size() > 0)
            {
                shared_ptr<lys_node_leaf*[]> k(
                        new lys_node_leaf*[keys.size()]());
                storeVoidPointer((shared_ptr<void>)k);
                list->keys = k.get();
            }
            // set up keys to contain just any leaf node
            else
            {
                shared_ptr<lys_node_leaf*[]> k(
                        new lys_node_leaf*[1]());
                storeVoidPointer((shared_ptr<void>)k);
                list->keys = k.get();
            }
            list->keys_size = 0;

            vector<sdfData*> properties =
                    data->getItemConstr()->getObjectProperties();

            // if the item constraints consist of an sdfRef
            if (properties.empty() && data->getItemConstr()->getReference())
            {
                openRefs.push_back(tuple<sdfCommon*, lys_node*>{
                    data->getItemConstr(), (lys_node*)list.get()});
            }
            for (int i = 0; i < properties.size(); i++)
            {
                childNode = sdfDataToNode(properties[i], childNode, module,
                        openRefs, openRefsType);
                if (childNode)
                    addNode(*childNode, (lys_node&)*list, module);

                // if the node was mentioned as an origin key
                // or if no keys were defined from conversion note, just take
                // first descendant leaf node as key
                if (find(keys.begin(), keys.end(), properties[i]->getName())
                        != keys.end()
                        || (keys.size() == 0 && list->keys_size == 0))
                {
                    if (childNode->nodetype == LYS_LEAF)
                    {
                        list->keys[list->keys_size++] =
                                (lys_node_leaf*)childNode;
                        if (keyNames == "")
                            keyNames += properties[i]->getName();
                        else
                            keyNames += " " + properties[i]->getName();

                        if (keys.size() == 0)
                            setSdfSpecExtension(childNode, "artificial-key");
                    }
                    else if (keys.size() > 0)
                        cerr << "sdfDataToNode: key node is not a leaf" << endl;
                }

            }
            if (!keyNames.empty())
                list->keys_str = storeString(keyNames);

            // ordered by user?
            if (orderedBy == "user")
                list->flags |= LYS_USERORDERED;
        }

        node = storeNode((shared_ptr<lys_node>&)list);
    }

    // type array with objects of type int, number, string or bool -> leaflist
    /*else if (data->getSimpType() == json_array
            && ((!data->getItemConstr()
               || data->getItemConstr()->getSimpType() != json_object)
               || (data->getSdfDataReference()
                       && (!data->getSdfDataReference()->getItemConstr()
                               || data->getSdfDataReference()->getItemConstr()
                               ->getSimpType() != json_object))))*/
    else if (data->getSimpType() == json_array
                && (!itemConstrWithRefs
                    || itemConstrWithRefs->getSimpType() != json_object
                    || origType == "bits" || origType == "empty"))
    {
        shared_ptr<lys_node_leaflist> leaflist =
                shared_ptr<lys_node_leaflist>(new lys_node_leaflist());
        leaflist->nodetype = LYS_LEAFLIST;
        leaflist->type.parent = (lys_tpdf*)leaflist.get();
        fillLysType(data->getItemConstr(), leaflist->type, openRefsType);

        if (data->getUnits() != "")
            leaflist->units = storeString(data->getUnits());
                              //data->getUnitsAsArray();
        //leaflist->dflt = (const char**)data->getDefaultAsCharArray();

        leaflist->min = data->getMinItems();
        leaflist->max = data->getMaxItems();

        vector<string> dfltVec = data->getDefaultArrayAsStringVector();
        // <<The "default" statement MUST NOT be present on nodes where
        // "min-elements" has a value greater than or equal to one.>>
        if (leaflist->min >= 1)
            cerr << "Nodes where min-elements has a value greater than or equal"
                    " to one cannot have default values in YANG."
                    " The default value(s) of '" + data->getName()
                    + "' will not be translated." << endl;
        else
        {
            shared_ptr<const char*[]> dflt(new const char*[dfltVec.size()]);
            voidPointerStore.push_back(dflt);
            leaflist->dflt = dflt.get();
            for (int i = 0; i < dfltVec.size(); i++)
                leaflist->dflt[i] = storeString(dfltVec[i]);
            leaflist->dflt_size = dfltVec.size();
        }

        node = storeNode((shared_ptr<lys_node>&)leaflist);
    }

    // type int, number, string, bool or no type
    else if ((data->getSimpType() != json_object
                || origType == "bits" || origType == "empty")
            && data->getSimpType() != json_array)
    {
        shared_ptr<lys_node_leaf> leaf =
                shared_ptr<lys_node_leaf>(new lys_node_leaf());
        leaf->nodetype = LYS_LEAF;

        leaf->type.parent = (lys_tpdf*)leaf.get();
        fillLysType(data, leaf->type, openRefsType);

        if (data->getUnits() != "")
            leaf->units = storeString(data->getUnits());
                          //data->getUnitsAsArray();
        leaf->dflt = storeString(data->getDefaultAsString());

        if (origUnique)
            leaf->flags |= LYS_UNIQUE;

        node = storeNode((shared_ptr<lys_node>&)leaf);
    }

    else
        cerr << "This should not happen" << endl;

    {
        shared_ptr<lys_node_choice> choice =
                make_shared<lys_node_choice>((lys_node_choice&)*node);
        choice->nodetype = LYS_CHOICE;

        // extract default from original node
        const char *dflt = NULL;
        if (node->nodetype == LYS_LEAF || node->nodetype == LYS_LEAFLIST)
            dflt = ((lys_node_leaf*)node)->dflt;

        lys_node *n;
        for (sdfData *c : data->getChoice())
        {
            shared_ptr<lys_node_case> caseP =
                    shared_ptr<lys_node_case>(new lys_node_case());

            caseP->nodetype = LYS_CASE;
            caseP->name = storeString(c->getName());

            n = sdfDataToNode(c, n, module, openRefs, openRefsType);

            if (n)
                addNode(*n, (lys_node&)*caseP, module);

            addNode((lys_node&)*caseP, (lys_node&)*choice, module);
            storeNode((shared_ptr<lys_node>&)caseP);
            node = storeNode((shared_ptr<lys_node>&)choice);
            // take into consideration what is currently overwritten
            // default etc.
            // Put default into each case
        }
    }

    if (!node->name)
        node->name = data->getNameAsArray();
    string dsc = removeConvNote(data->getDescription());
    node->dsc = storeString(dsc);

    if (origRef != "")
        node->ref = storeString(origRef);

    node->flags |= LYS_CONFIG_R;

    sdfRequiredToNode(data->getRequired(), module);

    pathsToNodes[data->generateReferenceString(NULL, true)] = node;

    // identityrefs are separately assigned so keep them out
    lys_node_leaf *l = (lys_node_leaf*)node;
    if (data->getReference()
        && ((l->nodetype != LYS_LEAF && l->nodetype != LYS_LEAFLIST) ||
            avoidNull(l->type.der->name) != "identityref"))
    {
        openRefs.push_back(tuple<sdfCommon*, lys_node*>{data, node});
    }

    if (origStatus == "DEPRECATED")
        node->flags |= LYS_STATUS_DEPRC;
    else if (origStatus == "OBSOLETE")
        node->flags |= LYS_STATUS_OBSLT;
    else if (origStatus == "CURRENT")
        node->flags |= LYS_STATUS_CURR;
    if (origConfig == "true")
    {
        node->flags |= LYS_CONFIG_W;
        node->flags |= LYS_CONFIG_SET;
    }
    else if (origConfig == "false")
    {
        node->flags |= LYS_CONFIG_R;
        node->flags |= LYS_CONFIG_SET;
    }
    if (data->getReadable() && data->getReadableDefined()
            && !data->getWritableDefined())
    {
        node->flags |= LYS_CONFIG_R;
        node->flags |= LYS_CONFIG_SET;
    }
    if (data->getWritable() && data->getWritableDefined())
    {
        node->flags |= LYS_CONFIG_W;
        node->flags |= LYS_CONFIG_SET;
    }
    if (!data->getWritable() && data->getWritableDefined())
    {
        node->flags |= LYS_CONFIG_R;
        node->flags |= LYS_CONFIG_SET;
    }
    if (!data->getReadable() && data->getReadableDefined())
    {
        setSdfSpecExtension(node, "readable false");
    }
    string b = "";
    if (data->getNullableDefined())
    {
        b = data->getNullable() ? "true" : "false";
        setSdfSpecExtension(node, "nullable " + b);
    }
    if (data->getObservableDefined())
    {
        b = data->getObservable() ? "true" : "false";
        setSdfSpecExtension(node, "observable " + b);
    }

    // if the node was originally just an augmention
    if (augByName != "")
    {
        lys_node_augment *aug = new lys_node_augment();
        string augmentRef = data->getParent()->generateReferenceString(true);
        for (int i = 0; i < openAugments.size(); i++)
        {
            if (get<2>(openAugments[i]) == augmentRef)
                aug = get<lys_node_augment*>(openAugments[i]);
        }
        if (!aug)
        {
            shared_ptr<lys_node_augment> augP(new lys_node_augment());
            storeNode((shared_ptr<lys_node>&)augP);
//            storeVoidPointer((shared_ptr<void>)augP);
            aug = augP.get();
        }
        aug->nodetype = LYS_AUGMENT;
        addNode(*node, (lys_node&)*aug, module);

        openAugments.push_back(tuple<string, lys_node_augment*, string>{
            augByName, aug, augmentRef});
        node = NULL;//(lys_node*)aug;
    }

    return node;
}

void convertDatatypes(vector<sdfData*> datatypes, lys_module &module,
        vector<tuple<sdfCommon*, lys_node*>> &openRefs,
        vector<tuple<sdfCommon*, lys_tpdf*>> &openRefsTpdf,
        vector<tuple<sdfCommon*, lys_type*>> &openRefsType)
{
    if (module.tpdf_size == 0)
    {
        // choose size better
        shared_ptr<lys_tpdf[]> tpdfs(new lys_tpdf[1000]());
        storeVoidPointer((shared_ptr<void>)tpdfs);
        module.tpdf = tpdfs.get();
    }
    if (module.ident_size == 0)
    {
        // choose size better
        shared_ptr<lys_ident[]> idents(new lys_ident[1000]());
        storeVoidPointer((shared_ptr<void>)idents);
        module.ident = idents.get();
    }
    sdfData *data;
    for (int i = 0; i < datatypes.size(); i++)
    {
        data = datatypes[i];
        vector<tuple<string, string>> origV = extractConvNote(data);
        string orig = "", origStatus = "", origRef = "", origContact = "",
                origDsc = "", origOrg = "", origConfig = "", augNs = "";
        vector<string> origRevs = {};
        int origAugSize = 0;
        for (int j = 0; j < origV.size(); j++)
        {
            if (get<1>(origV[j]) == "")
                orig = get<0>(origV[j]);
            else if (get<0>(origV[j]) == "status")
                origStatus = get<1>(origV[j]);
            else if (get<0>(origV[j]) == "config")
                origConfig = get<1>(origV[j]);
            else if (get<0>(origV[j]) == "reference")
                origRef = get<1>(origV[j]);
            else if (get<0>(origV[j]) == "organization")
                origOrg = get<1>(origV[j]);
            else if (get<0>(origV[j]) == "contact")
                origContact = get<1>(origV[j]);
            else if (get<0>(origV[j]) == "revision")
                origRevs.push_back(get<1>(origV[j]));
            else if (get<0>(origV[j]) == "augment_size")
                origAugSize = stoi(get<1>(origV[j]));
            else if (get<0>(origV[j]) == "augmented_ns")
                augNs = get<1>(origV[j]);

        }
        uint16_t flags = 0;
        if (origStatus == "DEPRECATED")
            flags |= LYS_STATUS_DEPRC;
        else if (origStatus == "OBSOLETE")
            flags |= LYS_STATUS_OBSLT;
        else if (origStatus == "CURRENT")
            flags |= LYS_STATUS_CURR;

        if (origConfig == "true")
        {
            flags |= LYS_CONFIG_W;
            flags |= LYS_CONFIG_SET;
        }
        else if (origConfig == "false")
        {
            flags |= LYS_CONFIG_R;
            flags |= LYS_CONFIG_SET;
        }
        if (data->getReadable() && data->getReadableDefined()
                && !data->getWritable())
        {
            flags |= LYS_CONFIG_R;
            flags |= LYS_CONFIG_SET;
        }
        if (data->getWritable() && data->getWritableDefined())
        {
            flags |= LYS_CONFIG_W;
            flags |= LYS_CONFIG_SET;
        }

        if (data->getParentFile() && data->getParentFile()->getInfo() &&
                data->getName() == data->getParentFile()->getInfo()->getTitle()
                                    + "-info")
        {
            module.org = storeString(origOrg);
            module.contact = storeString(origContact);

            if (origRevs.size() > 0)
            {
                shared_ptr<lys_revision[]> revs(
                        new lys_revision[origRevs.size()]());
                storeVoidPointer((shared_ptr<void>)revs);
                module.rev = revs.get();
                for (int j = 0; j < origRevs.size(); j++)
                    strcpy(module.rev[j].date,
                        storeString(origRevs[j]));
                module.rev_size = origRevs.size();
            }
            module.dsc = storeString(removeConvNote(data->getDescription()));
//            module.augment_size = origAugSize;
            module.augment_size = 0; // will be counted up later
            if (origAugSize > 0)
            {
                shared_ptr<lys_node_augment[]> augs(
                        new lys_node_augment[origAugSize]());
                storeVoidPointer((shared_ptr<void>)augs);
                module.augment = augs.get();
            }
        }

        else if (orig == "identity")
        {
            int s = module.ident_size;

            module.ident[s].name = storeString(data->getName());
            string dsc = removeConvNote(data->getDescription());
            module.ident[s].dsc = storeString(dsc);
            module.ident[s].module = &module;

            vector<sdfData*> op = data->getObjectProperties();
//            module.ident[s].base_size = op.size();
            sdfCommon *ref = data->getReference();
            shared_ptr<lys_ident*[]> bases(
                    new lys_ident*[ref? 1 : op.size()]());
            if (!op.empty() || ref)
            {
//                bases = make_shared<lys_ident*[]>(
//                        new lys_ident*[op.size()]());
                storeVoidPointer((shared_ptr<void>)bases);
                module.ident[s].base = bases.get();
            }

            module.ident[s].base_size = 0;

            for (int j = 0; j < op.size(); j++)
            {
                ref = op[j]->getReference();
                if (ref)
                {
                    shared_ptr<lys_ident> base(new lys_ident());
                    voidPointerStore.push_back((shared_ptr<void>)base);
                    module.ident[s].base[j] = base.get();
                    openBaseIdent.push_back(tuple<string, lys_ident**>{
                        ref->generateReferenceString(true),
                                &module.ident[s].base[j]});
                    module.ident[s].base_size++;
                    module.ident[s].base[j]->name = storeString("test");
                    module.ident[s].base[j]->module = &module;
                }
                else
                    cerr << "convertDatatypes: origin identity with multiple "
                            "bases but one sdfRef is null" << endl;
            }
            ref = data->getReference();
            if (ref)
            {
//                bases = make_shared<lys_ident*[]>(new lys_ident*[1]());
//                module.ident[s].base = bases.get();
//                storeVoidPointer((shared_ptr<void>)bases);

                shared_ptr<lys_ident> base(new lys_ident());
                voidPointerStore.push_back((shared_ptr<void>)base);
                module.ident[s].base[0] = base.get();
                openBaseIdent.push_back(tuple<string, lys_ident**>{
                    ref->generateReferenceString(true),
                            &module.ident[s].base[0]});
//                module.ident[s].base[0]->name = storeString("test");
//                module.ident[s].base[0]->module = &module;
//                module.ident[s].base_size = 1;
            }

            module.ident[s].flags |= flags;
            module.ident[s].ref = storeString(origRef);
            identStore[data->sdfCommon::generateReferenceString(true)] =
                    &module.ident[s];
            module.ident_size++;
        }
        // if the datatype is not of type array or object create a typedef
        else if ((data->getObjectPropertiesOfRefs().empty()
                && !data->getItemConstrOfRefs()
                && data->getChoice().empty()
                && data->getSimpType() != json_object
                && data->getSimpType() != json_array)
             || orig == "typedef")
        {
            module.tpdf[module.tpdf_size] = {};
            storeTypedef(&module.tpdf[module.tpdf_size]);
            sdfDataToTypedef(data, &module.tpdf[module.tpdf_size], module,
                    openRefsTpdf, openRefsType);

            module.tpdf[module.tpdf_size].flags |= flags;
            module.tpdf[module.tpdf_size].ref = storeString(origRef);
            module.tpdf_size++;
        }
        // else create a grouping
        else
        {
            shared_ptr<lys_node_grp> grp =
                    shared_ptr<lys_node_grp>(new lys_node_grp());
            lys_node *child  = sdfDataToNode(data, child, module, openRefs,
                    openRefsType);

            if (child && child->nodetype == LYS_CONTAINER)
            {
                grp = make_shared<lys_node_grp>((lys_node_grp&)*child);
                for (lys_node *n = child->child; n; n = n->next)
                    n->parent = (lys_node*)grp.get();
            }
            else if (child)
            {
                addNode(*child, (lys_node&)*grp, module);
            }

            grp->nodetype = LYS_GROUPING;
            grp->name = storeString(data->getName());

            addNode(*storeNode((shared_ptr<lys_node>&)grp), module);
            pathsToNodes[data->generateReferenceString(NULL, true)] =
                    (lys_node*)grp.get();

            setSdfSpecExtension((lys_node*)grp.get(), "sdfData");
            sdfRequiredToNode(data->getRequired(), module);

            // replace the entry in openRefs
            if (data->getReference())
            {
                auto it = openRefs.begin();
                for (int i = 0; i < openRefs.size(); i++)
                {
                    if (get<sdfCommon*>(openRefs[i]) == data)
                        openRefs.erase(it);
                    it++;
                }
                openRefs.push_back(tuple<sdfCommon*, lys_node*>{
                    data, (lys_node*)grp.get()});
            }
        }
    }

    // question (answered): should the typedef always be on top-level so it
    // can be reached from everywhere (scoping)? yes
}

void convertInfoBlock(sdfInfoBlock *info, lys_module &module)
{
    if (!info)
        return;
    // TITLE has to be altered
    string title = info->getTitle();
    // Spaces are replaced by '-'
    replace(title.begin(), title.end(), ' ', '-');
    // All special characters (like '(') have to be removed from the title
    title.resize(remove_if(title.begin(), title.end(),
            [](char x){return !isalnum(x) && x!='-';})-title.begin());
    // All characters in the title have to be lower case
    for (int i = 0; i < title.length(); i++)
        title[i] = tolower(title[i]);
    module.name = storeString(title);

    // DESCRIPTION
    // copyright and license of SDF are put into the module's description
    string dsc = avoidNull(module.dsc);
            + "\n\nCopyright: " + info->getCopyright()
            + "\nLicense: " + info->getLicense();
    module.dsc = storeString(dsc);

    // COPYRIGHT
//    module.org = info->getCopyrightAsArray();

    // VERSION (if not already copied from origin)
    if (module.rev_size == 0 && info->getVersion() != "")
    {
        shared_ptr<lys_revision[]> rev(new lys_revision[1]());
        storeVoidPointer((shared_ptr<void>)rev);
        module.rev = rev.get();
        strncpy(module.rev[0].date, storeString(info->getVersion()),
                sizeof(module.rev[0].date));
        module.rev_size = 1;
    }
}

void convertNamespaceSection(sdfNamespaceSection *ns, lys_module &module,
        vector<tuple<sdfCommon*, lys_node*>> &openRefs,
        vector<tuple<sdfCommon*, lys_tpdf*>> &openRefsTpdf,
        vector<tuple<sdfCommon*, lys_type*>> &openRefsType)
{
//    if (!ns)
//        return;

    string prefixString = "", nsString = "";
    if (ns)
    {
        prefixString = ns->getNamespaceString();
        if (!ns->getNamespaces().empty() && prefixString == "")
            nsString = ns->getNamespaces().begin()->second;
        else if(!ns->getNamespaces().empty())
            nsString = ns->getNamespaces()[prefixString];

    }

    if (prefixString == "")
        prefixString = module.name;

    module.prefix = storeString(prefixString);

    if (nsString == "")
        module.ns = storeString("urn:ietf:params:xml:ns:yang:"
            + prefixString);
    else
        module.ns = storeString(nsString);

    // import other mentioned namespaces
    if (ns)
    {
        map<string, sdfFile*> namedFiles = ns->getNamedFiles();
        map<string, sdfFile*>::iterator it;
        for (it = namedFiles.begin(); it != namedFiles.end(); it++)
        {
            // if the named file is not the one being converted at the moment
            // i.e. a foreign one
            if (it->second && it->first != ns->getDefaultNamespace())
            {
                lys_module *impMod = NULL;
                // if refTopFile has not already been converted convert it now
                for (int i = 0; i < fileToModule.size(); i++)
                {
                    if (get<sdfFile*>(fileToModule[i])->getInfo()->getTitle()
                            == it->second->getInfo()->getTitle())
                    {
                        impMod = get<lys_module*>(fileToModule[i]);
                        break;
                    }
                }

                if (!impMod)
                {
                    shared_ptr<lys_module> m(new lys_module());
                    storeVoidPointer((shared_ptr<void>)m);
                    m->ctx = module.ctx;
                    impMod = m.get();
    //                fileToModule.push_back(tuple<sdfFile*, lys_module*>{
    //                    it->second, impMod});

                    sdfFileToModule(*it->second, *m, openRefs, openRefsTpdf,
                            openRefsType);
                }

                bool alreadyImported = false;
                for (int i = 0; i< module.imp_size; i++)
                    if (impMod == module.imp[i].module)
                        alreadyImported = true;

                if (!alreadyImported && strcmp(impMod->name, module.name) != 0)
                {
                    shared_ptr<lys_import[]> imp(
                            new lys_import[module.imp_size+1]());
                    for (int i = 0; i < module.imp_size; i++)
                        imp[i] = module.imp[i];
                    storeVoidPointer((shared_ptr<void>)imp);
                    imp[module.imp_size].module = impMod;
                    imp[module.imp_size].prefix = impMod->prefix;
                    strcpy(imp[module.imp_size].rev, impMod->rev->date);
                    module.imp = imp.get();
                    module.imp_size++;
                }
            }
        }
    }
}

void assignOpenRefs(lys_module &module,
        vector<tuple<sdfCommon*, lys_node*>> &openRefs,
        vector<tuple<sdfCommon*, lys_tpdf*>> &openRefsTpdf,
        vector<tuple<sdfCommon*, lys_type*>> &openRefsType)
{

    // assign augments to the right module
    string str;
    lys_node_augment *aug;
    lys_module *mod;
    string targetRef = "";
    lys_node *target;
    for (int i = 0; i < openAugments.size(); i++)
    {
        tie(str, aug, targetRef) = openAugments.at(i);

        for (int j = 0; j < fileToModule.size(); j++)
        {
            // if the current module is being augmented
            if (avoidNull(module.name) == str)
            {
                mod = &module;
                // to only do the for loop once:
                j = fileToModule.size();
            }
            else
                mod = get<lys_module*>(fileToModule.at(j));

            if (avoidNull(mod->name) == str && mod->augment != NULL)
            {
                removeNode((lys_node&)*aug);
                mod->augment[mod->augment_size] = *aug;
                aug = &mod->augment[mod->augment_size];
                target = pathsToNodes[targetRef];
                aug->target = target;
                aug->target_name = storeString(
                        generatePath(target, target->module, true));
                aug->module = mod;
                for (lys_node *n = aug->child; n; n = n->next)
                    n->parent = (lys_node*)&mod->augment[mod->augment_size];

//                aug = NULL;
                mod->augment_size++;
                openAugments.erase(openAugments.begin() + i--);
            }
        }
    }

    // assign open references in nodes
    sdfCommon *c;
    lys_node *n;
//    vector<tuple<sdfCommon*, lys_node*>> stillLeft = {};
    for (int i = 0; i < openRefs.size(); i++)
    {
        tie(c, n) = openRefs[i];
        if (c && n && sdfRefToNode(c, n, module))
            openRefs.erase(openRefs.begin() + i--);
//            stillLeft.push_back(openRefs[i]);
    }
//    openRefs = stillLeft;

    // assign open references in typedefs
    lys_tpdf* t = NULL;
//    vector<tuple<sdfCommon*, lys_tpdf*>> stillLeftTpdf = {};
    for (int i = 0; i < openRefsTpdf.size(); i++)
    {
        tie(c, t) = openRefsTpdf[i];
        if (c && t && sdfRefToNode(c, (lys_node*)t, module, true))
            openRefsTpdf.erase(openRefsTpdf.begin() + i--);
            //stillLeftTpdf.push_back(openRefsTpdf[i]);
    }
//    openRefsTpdf = stillLeftTpdf;

    // assign open references in typedefs
    lys_type* t2 = NULL;
    for (int i = 0; i < openRefsType.size(); i++)
    {
        tie(c, t2) = openRefsType[i];
        if (c && t2)
        {
            if (c && t2 && sdfRefToType(
                    c->getReference()->generateReferenceString(true), t2))
            {
                //stillLeftType.push_back(openRefsType[i]);
                openRefsType.erase(openRefsType.begin() + i--);
            }
        }
    }

    lys_ident **id;
    for (int i = 0; i < openBaseIdent.size(); i++)
    {
        tie(str, id) = openBaseIdent[i];
        if (identStore[str])
        {
            *id = identStore[str];
            openBaseIdent.erase(openBaseIdent.begin() + i--);
        }
    }

    if (!openRefs.empty())
        cerr << "There is/are " + to_string(openRefs.size())
        + " node(s) with an unassigned reference in module "
        + avoidNull(module.name) << endl;

    if (!openRefsTpdf.empty())
        cerr << "There is/are " + to_string(openRefsTpdf.size())
        + " typedef(s) with an unassigned reference in module "
        + avoidNull(module.name) << endl;

    if (!openRefsType.empty())
        cerr << "There is/are " + to_string(openRefsType.size())
        + " type(s) with an unassigned reference in module "
        + avoidNull(module.name) << endl;

    if (!openBaseIdent.empty())
        cerr << "There is/are " + to_string(openBaseIdent.size())
        + " identit[y|ies] with an unassigned base identity in module "
        + avoidNull(module.name) << endl;

    if (!openAugments.empty())
        cerr << "There is/are " + to_string(openAugments.size())
        + " augment(s) with an unassigned module in module "
        + avoidNull(module.name) << endl;

    if (openRefs.empty() && openRefsTpdf.empty() && openRefsType.empty()
            && openBaseIdent.empty() && openAugments.empty())
        cout << "All references resolved after conversion of module "
            + avoidNull(module.name) << endl;
}

void importHelper(lys_module &module)
{
    module.imp_size += 1;
    shared_ptr<lys_import[]> imp(new lys_import[1]());
    module.imp = (lys_import*)storeVoidPointer((shared_ptr<void>)imp);
    module.imp[0].module = helper;
    module.imp[0].prefix = helper->prefix;
}

void convertProperties(vector<sdfProperty*> props, lys_module &module,
        vector<tuple<sdfCommon*, lys_node*>> &openRefs,
        vector<tuple<sdfCommon*, lys_type*>> &openRefsType)
{
    lys_node *currentNode;
    for (int i = 0; i < props.size(); i++)
    {
        currentNode = NULL;
        currentNode = sdfDataToNode(props[i], currentNode, module, openRefs,
                openRefsType);
        if (currentNode)
        {
            // add the current node into the tree
            addNode(*currentNode, module);
        }
        sdfRequiredToNode(props[i]->getRequired(), module);
    }
}

// now all actions are added to the module (which is changed later)
void convertActions(vector<sdfAction*> actions, lys_module &module,
        vector<tuple<sdfCommon*, lys_node*>> &openRefs,
        vector<tuple<sdfCommon*, lys_tpdf*>> &openRefsTpdf,
        vector<tuple<sdfCommon*, lys_type*>> &openRefsType)
{
    for (int i = 0; i < actions.size(); i++)
    {
        shared_ptr<lys_node_rpc_action> action(new lys_node_rpc_action());
        action->name = storeString(actions[i]->getName());
        action->dsc = storeString(actions[i]->getDescription());

        // if the action is not at top level translate to yang action
        if (!actions[i]->getParentFile())
            action->nodetype = LYS_ACTION;
        // else to rpc
        else
            action->nodetype = LYS_RPC;

        convertDatatypes(actions[i]->getDatatypes(), module, openRefs,
                openRefsTpdf, openRefsType);

        shared_ptr<lys_node_inout> input(new lys_node_inout());
        input->nodetype = LYS_INPUT;
        storeNode((shared_ptr<lys_node>&)input);
        addNode((lys_node&)*input, (lys_node&)*action, module);

        sdfData *inData = actions[i]->getInputData();
        if (inData)
        {
            lys_node *inputChild = sdfDataToNode(inData, inputChild, module,
                    openRefs, openRefsType);
            if(inputChild && inputChild->nodetype == LYS_CONTAINER)
            {
                input->child = inputChild->child;
                for (lys_node *sib = input->child; sib; sib = sib->next)
                    sib->parent = (lys_node*)input.get();
            }
            else if (inputChild)
            {
                inputChild->name = "input";
                addNode(*inputChild, (lys_node&)*input, module);
            }
        }
        else
            input->flags |= LYS_IMPLICIT;

        shared_ptr<lys_node_inout> output(new lys_node_inout());
        output->nodetype = LYS_OUTPUT;

        storeNode((shared_ptr<lys_node>&)output);
        addNode((lys_node&)*output, (lys_node&)*action, module);

        sdfData *outData = actions[i]->getOutputData();
        if (outData)
        {
            lys_node *outputChild = sdfDataToNode(outData, outputChild, module,
                    openRefs, openRefsType);
            if(outputChild && outputChild->nodetype == LYS_CONTAINER)
            {
                output->child = outputChild->child;
                for (lys_node *sib = output->child; sib; sib = sib->next)
                    sib->parent = (lys_node*)output.get();
            }
            else if (outputChild)
            {
                outputChild->name = "output";
                addNode(*outputChild, (lys_node&)*output, module);
            }
        }
        else
            output->flags |= LYS_IMPLICIT;

        setSdfSpecExtension((lys_node*)action.get(), "sdfAction");
        storeNode((shared_ptr<lys_node>&)action);
        addNode((lys_node&)*action, module);

        sdfRequiredToNode(actions[i]->getRequired(), module);
    }
}

void convertEvents(vector<sdfEvent*> events, lys_module &module,
        vector<tuple<sdfCommon*, lys_node*>> &openRefs,
        vector<tuple<sdfCommon*, lys_tpdf*>> &openRefsTpdf,
        vector<tuple<sdfCommon*, lys_type*>> &openRefsType)
{
    for (int i = 0; i < events.size(); i++)
    {
        shared_ptr<lys_node_notif> notif(new lys_node_notif());
        notif->name = storeString(events[i]->getName());
        notif->dsc = storeString(events[i]->getDescription());
        notif->nodetype = LYS_NOTIF;

        convertDatatypes(events[i]->getDatatypes(), module, openRefs,
                openRefsTpdf, openRefsType);

        sdfData *outData = events[i]->getOutputData();
        if (outData)
        {
            lys_node *outputChild = sdfDataToNode(outData, outputChild, module,
                    openRefs, openRefsType);

            if (outputChild && outputChild->nodetype == LYS_CONTAINER)
            {
                notif->child = outputChild->child;
                for (lys_node *sib = notif->child; sib; sib = sib->next)
                    sib->parent = (lys_node*)notif.get();
            }
            else if (outputChild)
            {
                outputChild->name = "output";
                addNode(*outputChild, (lys_node&)*notif, module);
            }
        }

        setSdfSpecExtension((lys_node*)notif.get(), "sdfEvent");
        storeNode((shared_ptr<lys_node>&)notif);
        addNode((lys_node&)*notif, module);

        sdfRequiredToNode(events[i]->getRequired(), module);
    }
}

struct lys_module* sdfThingToModule(sdfThing &thing, lys_module &module,
        vector<tuple<sdfCommon*, lys_node*>> &openRefs,
        vector<tuple<sdfCommon*, lys_tpdf*>> &openRefsTpdf,
        vector<tuple<sdfCommon*, lys_type*>> &openRefsType);

vector<lys_node*> convertThings(vector<sdfThing*> things, lys_module &module,
        vector<tuple<sdfCommon*, lys_node*>> &openRefs,
        vector<tuple<sdfCommon*, lys_tpdf*>> &openRefsTpdf,
        vector<tuple<sdfCommon*, lys_type*>> &openRefsType)
{
    vector<lys_node*> conts;

    for (int i = 0; i < things.size(); i++)
    {
        sdfThingToModule(*things[i], module, openRefs, openRefsTpdf,
                openRefsType);

        vector<tuple<string, string>> origV = extractConvNote(things[i]);
        string origPresence = "";
        for (int i = 0; i < origV.size(); i++)
        {
            if (get<0>(origV[i]) == "presence")
                origPresence = get<1>(origV[i]);
        }

        shared_ptr<lys_node_container> cont(new lys_node_container());
        cont->nodetype = LYS_CONTAINER;
        cont->presence = storeString(origPresence);
        cont->name = storeString(things[i]->getName());

        string dsc = removeConvNote(things[i]->getDescription());
        cont->dsc = storeString(dsc);

        cont->ref = storeString(avoidNull(module.ref));

        // take groupings to the top level
        lys_node *next;
        for (lys_node *n = module.data; n; n = next)
        {
            next =  n->next;
            if (n->nodetype == LYS_GROUPING)
            {
                removeNode(*n);
                conts.push_back(n);
            }
        }
        cont->child = module.data;
        module.data = NULL;

        for (lys_node *n = cont->child; n != NULL; n = n->next)
            n->parent = (lys_node*)cont.get();

        setSdfSpecExtension((lys_node*)cont.get(), "sdfThing");

        conts.push_back(storeNode((shared_ptr<lys_node>&)cont));

        pathsToNodes[things[i]->generateReferenceString(NULL, true)] =
                (lys_node*)cont.get();

        if (things[i]->getReference())
        {
            openRefs.push_back(tuple<sdfCommon*, lys_node*>{
                                things[i], (lys_node*)cont.get()});
        }
    }
    return conts;
}

struct lys_module* sdfObjectToModule(sdfObject &object, lys_module &module,
        vector<tuple<sdfCommon*, lys_node*>> &openRefs,
        vector<tuple<sdfCommon*, lys_tpdf*>> &openRefsTpdf,
        vector<tuple<sdfCommon*, lys_type*>> &openRefsType)
{
    vector<tuple<string, string>> origV = extractConvNote(&object);
    string origRef = "", origOrg = "", origContact = "";
    for (int i = 0; i < origV.size(); i++)
    {
        if (get<0>(origV[i]) == "reference")
            origRef = get<1>(origV[i]);
        else if (get<0>(origV[i]) == "organization")
            origOrg = get<1>(origV[i]);
        else if (get<0>(origV[i]) == "contact")
            origContact = get<1>(origV[i]);
    }

    module.type = 0;
    module.deviated = 0;
    module.inc_size = 0;
    module.ident_size = 0;
    module.features_size = 0;
    module.deviation_size = 0;
    module.extensions_size = 0;
    module.ref = storeString(origRef);
    module.org = storeString(origOrg);
    module.contact = storeString(origContact);
    module.version = YANG_VERSION;

    if (!object.getParentThing() && !object.getParentFile())
    {
        importHelper(module);
        openRefs.reserve(500);
        openRefsTpdf.reserve(500);
        convertInfoBlock(object.getInfo(), module);
        convertNamespaceSection(object.getNamespace(), module,
                openRefs, openRefsTpdf, openRefsType);
    }

    module.name = storeString(object.getName());
    string dsc = removeConvNote(object.getDescription());
    module.dsc = storeString(dsc);

    convertDatatypes(object.getDatatypes(), module, openRefs, openRefsTpdf,
            openRefsType);
    convertProperties(object.getProperties(), module, openRefs, openRefsType);

    convertActions(object.getActions(), module, openRefs, openRefsTpdf,
            openRefsType);
    convertEvents(object.getEvents(), module, openRefs, openRefsTpdf,
            openRefsType);

    sdfRequiredToNode(object.getRequired(), module);

    // assign open references in nodes and typedefs
    if (!object.getParentThing() && !object.getParentFile())
    {
        assignOpenRefs(module, openRefs, openRefsTpdf, openRefsType);
    }

    return &module;
}

vector<lys_node*> convertObjects(vector<sdfObject*> objects, lys_module &module,
        vector<tuple<sdfCommon*, lys_node*>> &openRefs,
        vector<tuple<sdfCommon*, lys_tpdf*>> &openRefsTpdf,
        vector<tuple<sdfCommon*, lys_type*>> &openRefsType)
{
    vector<lys_node*> conts;

    for (int i = 0; i < objects.size(); i++)
    {
        sdfObjectToModule(*objects[i], module, openRefs, openRefsTpdf,
                openRefsType);

        vector<tuple<string, string>> origV = extractConvNote(objects[i]);
        string origPresence = "";
        for (int i = 0; i < origV.size(); i++)
        {
            if (get<0>(origV[i]) == "presence")
                origPresence = get<1>(origV[i]);
        }

        shared_ptr<lys_node_container> cont(new lys_node_container());
        cont->nodetype = LYS_CONTAINER;
        cont->presence = storeString(origPresence);
        cont->name = storeString(objects[i]->getName());

        string dsc = removeConvNote(objects[i]->getDescription());
        cont->dsc = storeString(dsc);
        cont->ref = storeString(avoidNull(module.ref));

        // take groupings to the top level
        lys_node *next;
        for (lys_node *n = module.data; n; n = next)
        {
            next = n->next;
            if (n->nodetype == LYS_GROUPING)
            {
                removeNode(*n);
                conts.push_back(n);
            }
        }

        cont->child = module.data;
        module.data = NULL;

        for (lys_node *n = cont->child; n != NULL; n = n->next)
            n->parent = (lys_node*)cont.get();

        setSdfSpecExtension((lys_node*)cont.get(), "sdfObject");

        conts.push_back(storeNode((shared_ptr<lys_node>&)cont));

        pathsToNodes[objects[i]->generateReferenceString(NULL, true)] =
                (lys_node*)cont.get();

        if (objects[i]->getReference())
        {
            openRefs.push_back(tuple<sdfCommon*, lys_node*>{
                                objects[i], (lys_node*)cont.get()});
        }
    }

    return conts;
}

/*
 * The information from a sdfThing is transferred into a YANG module
 */
struct lys_module* sdfThingToModule(sdfThing &thing, lys_module &module,
        vector<tuple<sdfCommon*, lys_node*>> &openRefs,
        vector<tuple<sdfCommon*, lys_tpdf*>> &openRefsTpdf,
        vector<tuple<sdfCommon*, lys_type*>> &openRefsType)
{
    vector<lys_node*> conts;

    if (!thing.getParentThing() && !thing.getParentFile())
    {
        importHelper(module);
        openRefs.reserve(500);
        openRefsTpdf.reserve(500);
        convertInfoBlock(thing.getInfo(), module);
        convertNamespaceSection(thing.getNamespace(), module, openRefs,
                openRefsTpdf, openRefsType);
    }

    vector<tuple<string, string>> origV = extractConvNote(&thing);
    string origRef = "", origOrg = "", origContact = "";
    for (int i = 0; i < origV.size(); i++)
    {
        if (get<0>(origV[i]) == "reference")
            origRef = get<1>(origV[i]);
        else if (get<0>(origV[i]) == "organization")
            origOrg = get<1>(origV[i]);
        else if (get<0>(origV[i]) == "contact")
            origContact = get<1>(origV[i]);
    }

    conts = convertThings(thing.getThings(), module, openRefs, openRefsTpdf,
            openRefsType);
    vector<lys_node*> contsTmp = convertObjects(thing.getObjects(), module,
            openRefs, openRefsTpdf, openRefsType);
    conts.insert(conts.end(), contsTmp.begin(), contsTmp.end());

    for (int i = 0; i < conts.size(); i++)
        addNode(*conts[i], module);

    module.ref = storeString(origRef);
    module.org = storeString(origOrg);
    module.contact = storeString(origContact);
    module.type = 0;
    module.deviated = 0;
    //.imp_size = 0;
    module.inc_size = 0;
    //module.ident_size = 0;
    module.features_size = 0;
//    module.augment_size = 0;
    module.deviation_size = 0;
    module.extensions_size = 0;
    //module.ext_size = 0;
    module.version = YANG_VERSION;

    module.name = storeString(thing.getName());
    string dsc = removeConvNote(thing.getDescription());
    module.dsc = storeString(dsc);
    sdfRequiredToNode(thing.getRequired(), module);

    // assign open references in nodes and typedefs
    if (!thing.getParentThing() && !thing.getParentFile())
    {
        assignOpenRefs(module, openRefs, openRefsTpdf, openRefsType);
    }

    return &module;
}

struct lys_module* sdfFileToModule(sdfFile &file, lys_module &module,
        vector<tuple<sdfCommon*, lys_node*>> &openRefs,
        vector<tuple<sdfCommon*, lys_tpdf*>> &openRefsTpdf,
        vector<tuple<sdfCommon*, lys_type*>> &openRefsType)
{
    module.name = storeString(file.getInfo()->getTitle());
    fileToModule.push_back(tuple<sdfFile*, lys_module*>{&file, &module});
    vector<lys_node*> conts;
    openRefs.reserve(500);
    openRefsTpdf.reserve(500);
    importHelper(module);
    convertInfoBlock(file.getInfo(), module);

    convertNamespaceSection(file.getNamespace(), module, openRefs, openRefsTpdf,
            openRefsType);

    conts = convertThings(file.getThings(), module, openRefs, openRefsTpdf,
            openRefsType);
    vector<lys_node*> contsTmp = convertObjects(file.getObjects(), module,
            openRefs, openRefsTpdf, openRefsType);
    conts.insert(conts.end(), contsTmp.begin(), contsTmp.end());

    for (int i = 0; i < conts.size(); i++)
        addNode(*conts[i], module);

    convertProperties(file.getProperties(), module, openRefs, openRefsType);
    convertActions(file.getActions(), module, openRefs, openRefsTpdf,
            openRefsType);
    convertEvents(file.getEvents(), module, openRefs, openRefsTpdf,
            openRefsType);
    convertDatatypes(file.getDatatypes(), module, openRefs, openRefsTpdf,
            openRefsType);

    module.type = 0;
    module.deviated = 0;
    module.inc_size = 0;
    module.features_size = 0;
    module.deviation_size = 0;
    module.extensions_size = 0;
    module.version = YANG_VERSION;

    assignOpenRefs(module, openRefs, openRefsTpdf, openRefsType);

    for (int i = 0; i < module.imp_size; i++)
    {
        lys_module *m = module.imp[i].module;

        if (m != helper)
        {
            string mFileName = outputDirString + string(m->name) + ".yang";
            const char * mFileNameChar = storeString(mFileName);

            cout << "Printing imported module to file " << mFileName << endl;
            if (lys_print_path(mFileNameChar, m, LYS_OUT_YANG, NULL,
                    0, 0) == 0)
                cout << "-> successful" << endl;
            else
                cerr << "-> failed: " << strerror(errno) << endl;

        }
    }

    return &module;
}


int main(int argc, const char** argv)
{
    string usage = "Usage:\n"
            + avoidNull(argv[0]) + " -f path/to/input/file "
                    "[[-o path/to/output/file] | "
                    "[-d path/to/output/directory/ [-o output_file_name]]] "
                    "[-c path/to/yang/directory]";
    if (argc < 2)
    {
        cerr << "Missing arguments\n" + usage << endl;
        return -1;
    }
    // regexs to check file formats
    std::regex yang_regex (".*\\.yang");
    std::regex sdf_json_regex (".*\\.sdf\\.json");

    const char *inputFileName = NULL;
    const char *outputFileName = NULL;
    const char *outputDir = NULL;
    ly_ctx *ctx = NULL;
    for (int i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "-c") == 0)
        {
            // load the required context
            ctx = ly_ctx_new(argv[i+1], 0);
        }

        else if (strcmp(argv[i], "-f") == 0)
            inputFileName = argv[i+1];

        else if (strcmp(argv[i], "-o") == 0)
            outputFileName = argv[i+1];

        else if (strcmp(argv[i], "-d") == 0)
            outputDir = argv[i+1];
    }

    if (!ctx)
    {
        // if context was not specified in arguments
        // just load output or else current directory as context

        if (outputDir)
        {
            ly_ctx_destroy(ctx, NULL);
            ctx = ly_ctx_new(outputDir, 0);
        }

        if (!ctx)
        {
            ly_ctx_destroy(ctx, NULL);
            ctx = ly_ctx_new(".", 0);
        }

        if (!ctx)
        {
            // Try loading the context from this directory with the usual name
            ly_ctx_destroy(ctx, NULL);
            ctx = ly_ctx_new("./yang", 0);
        }

        if (!ctx)
        {
            cerr << "Loading YANG context failed" << endl << endl;
            return -1;
        }
    }
    if (!inputFileName)
    {
        cerr << "No input file name specified\n" + usage << endl;
        return -1;
    }
    outputDirString = "";
    if (outputDir)
    {
        regex isPath(".*/.*");
        if (outputFileName && regex_match(outputFileName, isPath))
        {
            cerr << "If a path to an output directory is given the output file"
                    " name cannot also contain a path\n\n" + usage << endl;
            return -1;
        }

        regex isDir(".*/");
        outputDirString = string(outputDir);
        if (!regex_match(outputDir, isDir))
            outputDirString += "/";
    }

    // choose better sizes
    nodeStore.reserve(10000);
    stringStore.reserve(10000);
    revStore.reserve(500);
    tpdfStore.reserve(10000);
    restrStore.reserve(5000);
    voidPointerStore.reserve(5000);

    // check whether input file is a YANG file
    if (std::regex_match(inputFileName, yang_regex))
    {
        if (outputFileName && !std::regex_match(outputFileName, sdf_json_regex))
        {
            cerr << "Incorrect output file format\n" << endl << endl;
            //return -1;
        }

        cout << "Parsing YANG module ";
        // load the module
        const lys_module *module =
                lys_parse_path(ctx, inputFileName, LYS_IN_YANG);

        if (module == NULL)
        {
            cerr << "-> failed" << endl;
            return -1;
        }
        cout << "-> succeeded" << endl << endl
                << "Converting YANG model to SDF..." << endl;

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
//        sdfObject *moduleObject = moduleToSdfObject(module);
        sdfFile *moduleFile = moduleToSdfFile(
                const_cast<lys_module*>(module));
        cout << "-> finished" << endl << endl;
        string outputFileString;
        if (outputFileName)
            outputFileString = outputDirString + outputFileName;
        else
            outputFileString = outputDirString + avoidNull(module->name)
                                    + ".sdf.json";

        cout << "Storing SDF model to file " + outputFileString + "..." << endl;

//        moduleObject->objectToFile(outputFileString);
        moduleFile->toFile(outputFileString);
        cout << " -> successful" << endl << endl;

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

//        ly_ctx_destroy(ctx, NULL);
        cout << "DONE" << endl;
        return 0;
    }

    // check whether input file is a SDF file
    else if (std::regex_match(inputFileName, sdf_json_regex))
    {
        if (outputFileName && !std::regex_match(outputFileName, yang_regex))
        {
            cerr << "Incorrect output file format\n" << endl;
            //return -1;
        }

        sdfObject moduleObject;
        sdfThing moduleThing;
        sdfFile moduleSdf;
        lys_module module = {};
        module.ctx = ctx;

        // approximate sizes of store vectors
        unsigned int nodeApprox = moduleObject.getDatatypes().size()
                + moduleObject.getProperties().size()
                + moduleObject.getActions().size()
                + moduleObject.getEvents().size();
        nodeStore.reserve(10000);
        stringStore.reserve(10000);
        revStore.reserve(500);
        tpdfStore.reserve(10000);
        restrStore.reserve(5000);
        voidPointerStore.reserve(5000);
//        openRefs.reserve(500);
//        openRefsTpdf.reserve(500);

        cout << "Parsing YANG conversion helper module 'sdf_extension.yang'";
        helper = const_cast<lys_module*>(lys_parse_path(
                ctx, "sdf_extension.yang", LYS_IN_YANG));

        if (helper == NULL)
            cerr << "-> failed" << endl << endl;
        else
            cout << "-> succeeded" << endl << endl;

        cout << "Loading SDF file..." << endl;
        cout << endl;

        vector<tuple<sdfCommon*, lys_node*>> openRefs = {};
        vector<tuple<sdfCommon*, lys_tpdf*>> openRefsTpdf = {};
        vector<tuple<sdfCommon*, lys_type*>> openRefsType = {};
        if (moduleSdf.fromFile(inputFileName))
        {
            cout << "...loading " + string(inputFileName)
                    + " -> finished" << endl << endl;
            cout << "Converting SDF model to YANG " << endl;
            sdfFileToModule(moduleSdf, module, openRefs, openRefsTpdf,
                    openRefsType);
        }
        else if (moduleObject.fileToObject(inputFileName, true) != NULL)
        {
            cout << "Loading SDF JSON file -> finished" << endl << endl;
            cout << "Converting SDF model to YANG " << endl;
            sdfObjectToModule(moduleObject, module, openRefs, openRefsTpdf,
                    openRefsType);
        }
        else if (moduleThing.fileToThing(inputFileName) != NULL)
        {
            cout << "Loading SDF JSON file -> finished" << endl << endl;
            cout << "Converting SDF model to YANG " << endl;
            sdfThingToModule(moduleThing, module, openRefs, openRefsTpdf,
                    openRefsType);
        }
        else
        {
            cerr << "No sdfObject or sdfThing could be loaded from the "
                    "input file\n\nDONE" << endl;
            return -1;
        }
        cout << "-> finished" << endl << endl;

        string outputFileString;
        if (outputFileName)
        {
            cmatch cm;
            regex r("(.*)\\.yang");
            regex_match(outputFileName, cm, r);
            module.name = cm[1].str().c_str();
        }
        else
        {
            outputFileString = outputDirString + avoidNull(module.name)
                    + ".yang";
            outputFileName = outputFileString.c_str();
        }

        cout << "Printing to file " << outputFileString;
        if (lys_print_path(outputFileName, &module, LYS_OUT_YANG, NULL,
                0, 0) == 0)
            cout << "-> successful" << endl << endl;
        else
            cerr << "-> failed: " << strerror(errno) << endl << endl;

        // validate the model
//        cout << "Validation ";
//        if (lys_parse_path(ctx, outputFileName, LYS_IN_YANG))
//            cout << "-> successful" << endl << endl;
//        else
//            cerr << "-> failed" << endl << endl;

//        ly_ctx_destroy(ctx, NULL);
        cout << "DONE" << endl;
        return 0;
    }

    return -1;
}
