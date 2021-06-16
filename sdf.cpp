#include "sdf.hpp"

// for convenience
using json = nlohmann::json;
using nlohmann::json_schema::json_validator;
using namespace std;


map<string, sdfCommon*> existingDefinitons;
map<string, sdfCommon*> existingDefinitonsGlobal;
vector<tuple<string, sdfCommon*>> unassignedRefs;
vector<tuple<string, sdfCommon*>> unassignedReqs;
map<string, sdfFile*> prefixToFile;

bool contextLoaded = false;
bool isContext = true;

string jsonDTypeToString(jsonDataType type)
{
    switch (type)
    {
    case json_number:
        return "number";
        break;
    case json_string:
        return "string";
        break;
    case json_boolean:
        return "boolean";
        break;
    case json_integer:
        return "integer";
        break;
    case json_array:
        return "array";
        break;
    case json_object:
        return "object";
        break;
    case json_type_undef:
        return "";
        break;
    default:
        cerr << "jsonDTypeToString(): parameter is invalid type" << endl;
        return "";
        break;
    }
}

jsonDataType stringToJsonDType(string str)
{
    if (str == "number" || str == "decimal64")
        return json_number;
    else if (str == "string")
        return json_string;
    else if (str == "boolean" || str == "bool")
        return json_boolean;
    else if (str == "integer" || str == "int"
            || str == "int8" || str == "uint8"
            || str == "int16" || str == "uint16"
            || str == "int32" || str == "uint32"
            || str == "int64" || str == "uint64")
        return json_integer;
    else if (str == "array")
        return json_array;
    else if (str == "object")
        return json_object;

    return json_type_undef;
}

void loadContext(const char *path = ".")
{
    contextLoaded = true;
    isContext = true;

    cout << "Searching for SDF context files..." << endl;
    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir (path)) != NULL)
    {
        std::regex sdfRegex (".*\\.sdf\\.json");
        string fileName = "";
        vector<string> names;
        while ((ent = readdir (dir)) != NULL)
        {
            fileName = string(ent->d_name);
            if (regex_match(fileName, sdfRegex))
                names.push_back(fileName);
        }
        closedir (dir);

        string prefix = "";
        static shared_ptr<sdfFile[]> files(new sdfFile[names.size()]());
        for (int i = 0; i < names.size(); i++)
        {
            cout << "...found: " + names[i] << endl;
            files[i].fromFile(names[i]);
            prefix = files[i].getNamespace()->getDefaultNamespace();
            if (prefix != "")
                prefixToFile[prefix] = &files[i];
        }
        if (names.size() == 0)
            cout <<  "...no files found" << endl;

        cout << "-> finished" << endl << endl;

        // update named files in namespaces after all files are loaded
        for (int i = 0; i < names.size(); i++)
            files[i].getNamespace()->updateNamedFiles();
    }
    else
    {
        //could not open directory
        cerr << "-> failed: ";
        perror ("");
    }

    isContext = false;
}

sdfCommon* refToCommon(string ref, std::string nsPrefix)
{
    // Also try alternative ref strings
    regex sameNsRegex("#/(.*)");
    regex diffNsRegex(".*:/(.*)");
    //regex diffNsRegex("([\\w\\d]+):/.*");
    smatch sm;
    string refAlter = "";
    if (regex_match(ref, sm, sameNsRegex) && nsPrefix != "")
        refAlter = nsPrefix + ":/" + string(sm[1]);

    else if (regex_match(ref, sm, diffNsRegex))
        refAlter = "#/" + string(sm[1]);

//    cout << "!!!" << endl;
//    cout << ref << endl;
//    cout << nsPrefix << endl;
//    cout << refAlter << endl;

    // Look through definitions for ref / alternative ref
    if (existingDefinitonsGlobal[ref])
        return existingDefinitonsGlobal[ref];

    else if (existingDefinitonsGlobal[refAlter])
        return existingDefinitonsGlobal[refAlter];

    else if (existingDefinitons[ref])
        return existingDefinitons[ref];

    else if (existingDefinitons[refAlter])
        return existingDefinitons[ref];

//    else
//        cerr << "refToCommon(): definition for reference "
//                + ref + " not found (yet)" << endl;

    return NULL;
}

vector<tuple<string, sdfCommon*>> assignRefs(
        vector<tuple<string, sdfCommon*>> unassignedRefs, refOrReq r)
        //map<string, sdfCommon*> defs)
{
    // check for references left unassigned
    string name;
    sdfCommon *com;
    vector<tuple<string, sdfCommon*>> stillLeft = {};

    for (tuple<string, sdfCommon*> unRefs : unassignedRefs)
    {
        tie(name, com) = unRefs;
        sdfFile *file = com->getTopLevelFile();
        string nsPrefix = "";
        if (file && file->getNamespace())
            nsPrefix = file->getNamespace()->getDefaultNamespace();
        sdfCommon *ref = refToCommon(name, nsPrefix);

        if (com && ref)
        {
            if (r == REF)
                com->setReference(ref);
            else if (r == REQ)
                com->addRequired(ref);

            //cout << com->getName()+" refs "+ref->getName()<<endl;
        }

        vector<sdfCommon*> reqs = com->getRequired();
        if (!com || (r == REF && !com->getReference()) ||
                (r == REQ && find(reqs.begin(), reqs.end(), ref) == reqs.end()))
            stillLeft.push_back(unRefs);

        //cout << name << endl;
    }

    return stillLeft;
}

/*
 * Necessary because iterator.value() replaces _ with space (???)
 */
string correctValue(string val)
{
    string correct = val;
    for(int i = 0; i < correct.length(); i++)
    {
       if(isspace(correct[i]))
           correct[i] = '_';
    }
    return correct;
}

sdfCommon::sdfCommon(string _name, string _description, sdfCommon *_reference,
        vector<sdfCommon*> _required, sdfFile *_file)
            : description(_description), name(_name), reference(_reference),
              required(_required), parentFile(_file)
{
    label = "";
    //this->parent = NULL;
}

sdfCommon::~sdfCommon()
{
    // items in required cannot be deleted because they are deleted somewhere
    // else as sdfData pointers already which are somehow not affected by the
    // i = NULL command and then cause a segfault
    for (int i = 0; i < required.size(); i++)
    {
        //delete required[i];
        required[i] = NULL;
    }
    required.clear();

    // reference cannot be deleted, see above explanation
    reference = NULL;
    parentFile = NULL;
}

void sdfCommon::setLabel(string _label)
{
    this->label = _label;
}

string sdfCommon::getDescription()
{
    return description;
}

string sdfCommon::getLabel()
{
    return label;
}

const char* sdfCommon::getLabelAsArray()
{
    return label.c_str();
}

sdfCommon* sdfCommon::getReference()
{
    return this->reference;
}

/*
string sdfCommon::getReferenceAsString()
{
    // TODO: for the reference (not the common!) create a string
    // (e.g. "#/sdfObject/temperatureWithAlarm/sdfData/temperatureData")
    // or return empty string if there is no reference (try catch block?)
    return "";
}
*/
vector<sdfCommon*> sdfCommon::getRequired()
{
    return required;
}
/*
string sdfCommon::getPointer()
{

    return this->jsonPointer.to_string();
}
*/

sdfData* sdfCommon::getSdfDataReference() const
{
    return dynamic_cast<sdfData*>(reference);
}

sdfData* sdfCommon::getSdfPropertyReference() const
{
    return dynamic_cast<sdfProperty*>(reference);
}

void sdfCommon::addRequired(sdfCommon *common)
{
    this->required.push_back(common);
}

void sdfCommon::setReference(sdfCommon *common)
{
    this->reference = common;
}


json sdfCommon::commonToJson(json prefix)
{
    if (this->getReference())
    {
        bool imp = false;
        if (this->getReference()->getTopLevelFile() != this->getTopLevelFile())
            imp = true;
        prefix["sdfRef"] = this->getReference()->generateReferenceString(imp);
    }
    if (this->getLabel() != "")
        prefix["label"] = this->getLabel();
    if (this->getDescription() != "")
        prefix["description"] = this->getDescription();
    vector<string> req = {};
    for (sdfCommon *i : this->required)
    {
        req.push_back(i->generateReferenceString());
    }
    if (!req.empty())
        prefix["sdfRequired"] = req;

    return prefix;
}

sdfObjectElement::sdfObjectElement(string _name, string _description,
        sdfCommon *_reference, vector<sdfCommon*> _required,
        sdfObject *_parentObject)
            : sdfCommon(_name, _description, _reference, _required),
              parentObject(_parentObject)
{
}

sdfObjectElement::~sdfObjectElement()
{
    // if parentObject is deleted a loop is created
    parentObject = NULL;
}

sdfObject* sdfObjectElement::getParentObject() const
{
    return parentObject;
}

void sdfObjectElement::setParentObject(sdfObject *_parentObject)
{
    this->parentObject = _parentObject;
}

/*
string sdfObjectElement::generateReferenceString()
{
    if (this->getParentObject())
        return this->parentObject->generateReferenceString() + "/";

    else if (this->getParentFile())
        //return "#/";
        return this->getParentFile()->generateReferenceString();
    else
    {
        cerr << "sdfObjectElement::generateReferenceString(): "
                << this->getName() + " has no assigned parent object."
                << endl;
        return "";
    }
}*/

sdfInfoBlock::sdfInfoBlock(string _title, string _version, string _copyright,
        string _license)
            : title(_title), version(_version), copyright(_copyright),
              license(_license)
{}

string sdfInfoBlock::getTitle()
{
    return title;
}

string sdfInfoBlock::getVersion()
{
    return version;
}

string sdfInfoBlock::getCopyright()
{
    return copyright;
}

string sdfInfoBlock::getLicense()
{
    return license;
}

json sdfInfoBlock::infoToJson(json prefix)
{
    //cout << "info to json" << endl;
    prefix["info"]["title"] = this->getTitle();
    prefix["info"]["version"] = this->getVersion();
    prefix["info"]["copyright"] = this->getCopyright();
    prefix["info"]["license"] = this->getLicense();

    return prefix;
}

sdfNamespaceSection::sdfNamespaceSection()
{
    namespaces = map<string, string>();
    default_ns = "";
    namedFiles = map<string, sdfFile*>();
}

sdfNamespaceSection::sdfNamespaceSection(map<string, string> _namespaces,
        string _default_ns)
            : namespaces(_namespaces), default_ns(_default_ns)
{
    // link files no foreign namespaces
    map<string, string>::iterator it;
    for (it = namespaces.begin(); it != namespaces.end(); it++)
        namedFiles[it->first] = prefixToFile[it->first];

    if (!default_ns.empty())
        namedFiles[default_ns] = NULL;
}


map<string, string> sdfNamespaceSection::getNamespaces()
{
    return namespaces;
}

string sdfNamespaceSection::getDefaultNamespace()
{
    return default_ns;
}


std::string sdfNamespaceSection::getNamespaceString() const
{
    if (default_ns != "")
        return default_ns;
    else if (!namespaces.empty())
        return namespaces.begin()->first;
    else
        return "";
}

json sdfNamespaceSection::namespaceToJson(json prefix)
{
    //cout << "ns to json" << endl;
    for (auto it : this->namespaces)
        prefix["namespace"][it.first] = it.second;

    if (this->default_ns != "")
        prefix["defaultNamespace"] = this->default_ns;
    return prefix;
}

sdfData::sdfData() : sdfData("", "", "")
{}

sdfData::sdfData(string _name, string _description, string _type,
        sdfCommon *_reference, vector<sdfCommon*> _required,
        sdfCommon *_parentCommon, vector<sdfData*> _choice)
            : sdfCommon(_name, _description, _reference, _required),
              parent(_parentCommon)
{
    simpleType = stringToJsonDType(_type);
    derType = _type;

    this->setParentCommon(_parentCommon);

    readable = NAN;
    writable = NAN;
    observable = NAN;
    nullable = NAN;
    readableDefined = false;
    writableDefined = false;
    observableDefined = false;
    nullableDefined = false;
    subtype = sdf_subtype_undef;
    format = json_format_undef;
    constDefined = false;
    defaultDefined = false;
    constBoolDefined = false;
    defaultBoolDefined = false;
    constIntDefined = false;
    defaultIntDefined = false;
    constantBool = false;
    defaultBool = false;
    constantString = "";
    defaultString = "";
    constantNumber = NAN;
    defaultNumber = NAN;
    constantInt = -1;
    defaultInt = -1;
    defaultObject = NULL;
    constantObject = NULL;
    defaultBoolArray = {};
    constantBoolArray = {};
    defaultStringArray = {};
    constantStringArray = {};
    defaultNumberArray = {};
    constantNumberArray = {};
    defaultIntArray = {};
    constantIntArray = {};
    //constantAsCharArray = NULL;
    //defaultAsCharArray = NULL;
    exclusiveMaximum_bool = false;
    exclusiveMinimum_bool = false;
    minLength = NAN;
    maxLength = NAN;
    minimum = NAN;
    maximum = NAN;
    minInt = 0;
    maxInt = 0;
    minIntSet = false;
    maxIntSet = false;
    multipleOf = NAN;
    exclusiveMinimum_number = NAN;
    exclusiveMaximum_number = NAN;
    pattern = "";
    maxItems = NAN;
    minItems = NAN;
    uniqueItems = NAN;
    uniqueItemsDefined = false;
    item_constr = NULL;
    units = "";
    scaleMinimum = NAN;
    scaleMaximum = NAN;
    contentFormat = "";
    sdfChoice = _choice;
    objectProperties = {};
    requiredObjectProperties = {};
}

sdfData::sdfData(string _name, string _description, jsonDataType _type,
        sdfCommon *_reference, vector<sdfCommon*> _required,
        sdfCommon *_parentCommon, vector<sdfData*> _choice)
        : sdfData(_name, _description, jsonDTypeToString(_type), _reference,
        _required, _parentCommon, _choice)
{}

sdfData::sdfData(sdfData &data)
    : sdfData(data.getName(), data.getDescription(), data.getType(),
            data.getReference(), data.getRequired(), NULL, data.getChoice())
{
    readable = data.getReadable();
    writable = data.getWritable();
    observable = data.getObservable();
    nullable = data.getNullable();
    readableDefined = data.getReadableDefined();
    writableDefined = data.getWritableDefined();
    observableDefined = data.getObservableDefined();
    nullableDefined = data.getNullableDefined();
    subtype = data.getSubtype();
    format = data.getFormat();
    constDefined = data.getConstantDefined();
    defaultDefined = data.getDefaultDefined();
    constBoolDefined = data.getConstantBoolDefined();
    defaultBoolDefined = data.getDefaultBoolDefined();
    constIntDefined = data.getConstantIntDefined();
    defaultIntDefined = data.getDefaultIntDefined();
    constantBool = data.getConstantBool();
    defaultBool = data.getDefaultBool();
    constantString = data.getDefaultString();
    defaultString = data.getDefaultString();
    constantNumber = data.getConstantNumber();
    defaultNumber = data.getDefaultNumber();
    constantInt = data.getConstantInt();
    defaultInt = data.getDefaultInt();
    defaultObject = data.getDefaultObject();
    constantObject = data.getConstantObject();
    defaultBoolArray = data.getDefaultBoolArray();
    constantBoolArray = data.getConstantBoolArray();
    defaultStringArray = data.getDefaultStringArray();
    constantStringArray = data.getConstantStringArray();
    defaultNumberArray = data.getDefaultNumberArray();
    constantNumberArray = data.getConstantNumberArray();
    defaultIntArray = data.getDefaultIntArray();
    constantIntArray = data.getConstantIntArray();
    //constantAsCharArray = data.getConstantAsCharArray();
    //defaultAsCharArray = data.getDefaultAsCharArray();
    exclusiveMaximum_bool = false; // TODO
    exclusiveMinimum_bool = false; // TODO
    minLength = data.getMinLength();
    maxLength = data.getMaxLength();
    minimum = data.getMinimum();
    maximum = data.getMaximum();
    minInt = data.getMinInt();
    maxInt = data.getMaxInt();
    minIntSet = data.getMinIntSet();
    maxIntSet = data.getMaxIntSet();
    multipleOf = data.getMultipleOf();
    exclusiveMinimum_number = data.getExclusiveMinimumNumber();
    exclusiveMaximum_number = data.getExclusiveMaximumNumber();
    pattern = data.getPattern();
    maxItems = data.getMaxItems();
    minItems = data.getMinItems();
    uniqueItems = data.getUniqueItems();
    uniqueItemsDefined = data.getUniqueItemsDefined();
    if (data.getItemConstr())
    this->setItemConstr(data.getItemConstr()); // TODO: deep or shallow copy?
    //this->setItemConstr(new sdfData(*data.getItemConstr()));
    units = data.getUnits();
    scaleMinimum = data.getScaleMinimum();
    scaleMaximum = data.getScaleMaximum();
    contentFormat = data.getContentFormat();
    this->setChoice(data.getChoice()); // TODO: s.o.
    //for (sdfData *d : data.getChoice())
    //    this->addChoice(new sdfData(*d));
    this->setObjectProperties(data.getObjectProperties()); // TODO: s.o.
    //for (sdfData *d : data.getObjectProperties())
    //    this->addObjectProperty(new sdfData(*d));
    requiredObjectProperties = data.getRequiredObjectProperties();
}
sdfData::sdfData(sdfProperty &prop)
    : sdfData((sdfData&)prop)
{
}

sdfData::~sdfData()
{
    for (int i = 0; i < objectProperties.size(); i++)
    {
        delete objectProperties[i];
        objectProperties[i] = NULL;
    }
    objectProperties.clear();

    for (int i = 0; i < sdfChoice.size(); i++)
    {
        delete sdfChoice[i];
        sdfChoice[i] = NULL;
    }
    sdfChoice.clear();

    //delete[] objectProperties.data();
    //delete[] sdfChoice.data();

    delete item_constr;
    item_constr = NULL;

    // if parent was deleted a loop would be created
    parent = NULL;
}

void sdfData::setNumberData(float _constant,
        float _default, float _min, float _max, float _multipleOf)
{
    if (this->simpleType != json_number && simpleType != json_type_undef)
    {
        cerr << this->getName() + " cannot be instantiated as type number"
                + " because it already has a different type. "
                + "Please use setType() first to reset it." << endl;
        return;
    }
    if (_min > _max)
        cerr << "Selected minimum value is greater than selected "
                "maximum value: " << _min << " > " << _max << endl;
    this->setType(json_number);
    //enumNumber = _enum;
    constantNumber = _constant;
    defaultNumber = _default;
    minimum = _min;
    maximum = _max;
    multipleOf = _multipleOf;
    if(isnan(_constant))
        constDefined = false;
    else
        constDefined = true;
    if (isnan(_default))
        defaultDefined = false;
    else
        defaultDefined = true;
}

void sdfData::setStringData(string _constant, string _default,
        float _minLength, float _maxLength, string _pattern,
        jsonSchemaFormat _format, vector<string> _enum)
{
    if (simpleType != json_string && simpleType != json_type_undef)
    {
        cerr << this->getName() + " cannot be instantiated as type string"
                + " because it already has a different type. "
                + "Please use setType() first to reset it." << endl;
        return;
    }
    this->setType(json_string);
    enumString = _enum;
    constantString = _constant;
    defaultString = _default;
    minLength = _minLength;
    maxLength = _maxLength;
    pattern = _pattern;
    format = _format;
    if(_constant == "")
        constDefined = false;
    else
        constDefined = true;
    if (_default == "")
        defaultDefined = false;
    else
        defaultDefined = true;
}

void sdfData::setBoolData(bool _constant, bool defineConst, bool _default,
        bool defineDefault)
{
    if (simpleType != json_boolean && simpleType != json_type_undef)
    {
        cerr << this->getName() + " cannot be instantiated as type boolean"
                + " because it already has a different type. "
                + "Please use setType() first to reset it." << endl;
        return;
    }
    this->setType(json_boolean);
    //enumBool = _enum;
    constDefined = defineConst;
    constBoolDefined = defineConst;
    constantBool = _constant;
    defaultDefined = defineDefault;
    defaultBoolDefined = defineDefault;
    defaultBool = _default;
}

void sdfData::setIntData(int _constant, bool defineConst, int _default,
        bool defineDefault, float _min, float _max)
{
    if (simpleType != json_integer && simpleType != json_type_undef)
    {
        cerr << this->getName() + " cannot be instantiated as type integer"
                + " because it already has a different type. "
                + "Please use setType() first to reset it." << endl;
        return;
    }
    if (_min > _max)
        cerr << "Selected minimum value is greater than selected "
                "maximum value: " << _min << " > " << _max << endl;
    this->setType(json_integer);
    //enumInt = _enum;
    constDefined = defineConst;
    constIntDefined = defineConst;
    constantInt = _constant;
    defaultDefined = defineDefault;
    defaultIntDefined = defineDefault;
    defaultInt = _default;
    minimum = _min;
    maximum = _max;
}

void sdfData::setArrayData(float _minItems, float _maxItems,
        bool _uniqueItems, sdfData *_itemConstr)
{
    if (simpleType != json_array && simpleType != json_type_undef)
    {
        cerr << this->getName() + " cannot be instantiated as type array"
                + " because it already has a different type. "
                + "Please use setType() first to reset it." << endl;
        return;
    }
    if (_itemConstr != NULL && _itemConstr->getType() == "array")
    {
        cerr << "setArrayData(): arrays cannot have items of type array"
                << endl;
        //return;
    }
    if (_itemConstr != NULL &&
            (_itemConstr->constDefined
            || _itemConstr->defaultDefined
            || _itemConstr->pattern != ""
            || !isnan(_itemConstr->multipleOf)
            || !isnan(_itemConstr->exclusiveMaximum_number)
            || !isnan(_itemConstr->exclusiveMinimum_number)
            || _itemConstr->exclusiveMaximum_bool
            || _itemConstr->exclusiveMinimum_bool))
        cerr << "setArrayData(): item constraints contain invalid attributes"
        << endl;
    this->setType(json_array);
    minItems = _minItems;
    maxItems = _maxItems;
    uniqueItems = _uniqueItems;
    item_constr = _itemConstr;
}
/*
void sdfData::setArrayData(vector<string> enm, float _minItems,
        float _maxItems, bool _uniqueItems, string item_type, sdfCommon *ref,
        float minLength, float maxLength, jsonSchemaFormat format)
{
    this->setArrayData(_minItems, _maxItems, _uniqueItems, item_type, ref);

    item_constr = new sdfData("", "", item_type, ref);
    if (item_constr->getSimpType() == json_string)
    {
        item_constr->setStringData("", "", minLength, maxLength,
                "", format, enm);
    }
}

void sdfData::setArrayData(vector<float> enm, float _minItems,
        float _maxItems, bool _uniqueItems, string item_type, sdfCommon *ref,
        float min, float max)
{
    this->setArrayData(_minItems, _maxItems, _uniqueItems, item_type, ref);

    item_constr = new sdfData("", "", item_type, ref);
    if (item_constr->getSimpType() == json_number)
    {
        item_constr->setNumberData(NAN, NAN, min, max, NAN, enm);
    }
}

void sdfData::setArrayData(vector<int> enm, float _minItems,
        float _maxItems, bool _uniqueItems, string item_type, sdfCommon *ref,
        float min, float max)
{
    this->setArrayData(_minItems, _maxItems, _uniqueItems, item_type, ref);

    item_constr = new sdfData("", "", item_type, ref);
    if (item_constr->getSimpType() == json_integer)
    {
        item_constr->setIntData(-1, false, -1, false, min, max, enm);
    }
}
*/
void sdfData::setUnits(string _units, float _scaleMin, float _scaleMax)
{
    units = _units;
    scaleMinimum = _scaleMin;
    scaleMaximum = _scaleMax;
}

void sdfData::setReadWrite(bool _readable, bool _writable)
{
    readable = _readable;
    writable = _writable;
}

void sdfData::setObserveNull(bool _observable, bool _nullable)
{
    observable = _observable;
    nullable = _nullable;
}

void sdfData::setFormat(jsonSchemaFormat _format)
{
    format = _format;
}

void sdfData::setSubtype(sdfSubtype _subtype)
{
    subtype = _subtype;
}

bool sdfData::getConstantBool()
{
    return constantBool;
}

int64_t sdfData::getConstantInt()
{
    return constantInt;
}

float sdfData::getConstantNumber()
{
    return constantNumber;
}

string sdfData::getConstantString()
{
    return constantString;
}

string sdfData::getContentFormat()
{
    return contentFormat;
}

bool sdfData::getDefaultBool()
{
    return defaultBool;
}

int64_t sdfData::getDefaultInt()
{
    return defaultInt;
}

float sdfData::getDefaultNumber()
{
    return defaultNumber;
}

string sdfData::getDefaultString()
{
    return defaultString;
}
/*
vector<bool> sdfData::getEnumBool()
{
    return enumBool;
}

vector<int> sdfData::getEnumInt()
{
    return enumInt;
}

vector<float> sdfData::getEnumNumber()
{
    return enumNumber;
}
*/
vector<string> sdfData::getEnumString()
{
    return enumString;
}

bool sdfData::isExclusiveMaximumBool()
{
    return exclusiveMaximum_bool;
}

float sdfData::getExclusiveMaximumNumber()
{
    return exclusiveMaximum_number;
}

bool sdfData::isExclusiveMinimumBool()
{
    return exclusiveMinimum_bool;
}

float sdfData::getExclusiveMinimumNumber()
{
    return exclusiveMinimum_number;
}

jsonSchemaFormat sdfData::getFormat()
{
    return format;
}

float sdfData::getMaximum()
{
    return maximum;
}

float sdfData::getMaxItems()
{
    return maxItems;
}

float sdfData::getMaxItemsOfRef()
{
    sdfData *ref = this->getSdfDataReference();
    if (isnan(maxItems) && ref)
        return ref->getMaxItemsOfRef();

    return maxItems;
}

float sdfData::getMaxLength()
{
    return maxLength;
}

float sdfData::getMinimum()
{
    return minimum;
}

float sdfData::getMinItems()
{
    return minItems;
}

float sdfData::getMinItemsOfRef()
{
    sdfData *ref = this->getSdfDataReference();
    if (isnan(minItems) && ref)
        return ref->getMinItemsOfRef();

    return minItems;
}

float sdfData::getMinLength()
{
    return minLength;
}

float sdfData::getMultipleOf()
{
    return multipleOf;
}

bool sdfData::getNullable()
{
    return nullable;
}

bool sdfData::getObservable()
{
    return observable;
}

string sdfData::getPattern()
{
    return pattern;
}

float sdfData::getScaleMaximum()
{
    return scaleMaximum;
}

float sdfData::getScaleMinimum()
{
    return scaleMinimum;
}

sdfSubtype sdfData::getSubtype()
{
    return subtype;
}

string sdfData::getType()
{
    if (this->getReference())
    {
//        if (simpleType != json_type_undef)
//            cerr << "sdfData::getType: " + this->getName() +
//            " has an sdfRef and a type given (sdfRef to "
//            + this->getReference()->getName() + " and own type "
//            + jsonDTypeToString(simpleType) + ")" << endl;

        sdfData *ref = dynamic_cast<sdfData*>(this->getReference());
        if (ref)
            return ref->getType();
        else
            cerr << "sdfData::getType: reference is of wrong type" << endl;
    }

    if (this->simpleType != json_type_undef)
        return jsonDTypeToString(simpleType);

    return this->derType;
}

jsonDataType sdfData::getSimpType()
{
    if (this->getReference())
    {
//        if (simpleType != json_type_undef)
//            cerr << "sdfData::getSimpType: " + this->getName() +
//            " has an sdfRef and a type given (sdfRef to "
//            + this->getReference()->getName() + " and own type "
//            + jsonDTypeToString(simpleType) + ")" << endl;

        sdfData *ref = dynamic_cast<sdfData*>(this->getReference());
        if (ref)
            return ref->getSimpType();
        else
        {
            cerr << "sdfData::getSimpType: reference is of wrong type" << endl;
            cout << this->getReference()->getName();
        }
    }

    // if all choices have the same type return that type
    jsonDataType choiceType;
    bool sameTypes = true;
    for (int i = 0; i < sdfChoice.size() && sameTypes; i++)
    {
        if (i == 0)
            choiceType = sdfChoice[0]->getSimpType();

        else if (sdfChoice[i]->getSimpType() != choiceType)
            sameTypes = false;
    }
    if (!sdfChoice.empty() && sameTypes)
        return choiceType;

    return simpleType;
}

bool sdfData::getUniqueItems()
{
    return uniqueItems;
}

string sdfData::getUnits()
{
    if (units != "")
        return units;

    else if (subtype == sdf_unix_time)
        return "unix-time";
    else if (format == json_date_time)
        return "date-time";
    else if (format == json_date)
        return "date";
    else if (format == json_time)
        return "time";
    else if (format == json_uri)
        return "uri";
    else if (format == json_uri_reference)
        return "uri-reference";
    else if (format == json_uuid)
        return "uuid";

    return "";
}

//bool sdfData::hasChild(sdfCommon *child) const
//{
//    if (item_constr && (item_constr == child
//            || find(item_constr->getObjectProperties().begin(),
//                    item_constr->getObjectProperties().end(), child)
//                    != item_constr->getObjectProperties().end()))
//        return true;
//
//    else if (find(objectProperties.begin(), objectProperties.end(), child)
//            != objectProperties.end())
//        return true;
//    else if (find(sdfChoice.begin(), sdfChoice.end(),  child)
//            != sdfChoice.end())
//        return true;
//
//    return false;
//}

string sdfData::generateReferenceString(sdfCommon *child, bool import)
{
    if (!child)
        return this->sdfCommon::generateReferenceString(import);

    sdfCommon *parent = this->getParent();
    sdfFile *parentFile = this->getParentFile();
    string childRef = "";

    if (item_constr == child)
    {
        childRef = "";
    }
    // else if child is part of the objectProperties/choices/itemConstraint
    else if (find(objectProperties.begin(), objectProperties.end(), child)
            != objectProperties.end()
            || find(sdfChoice.begin(), sdfChoice.end(),  child)
            != sdfChoice.end()
            || (item_constr && find(item_constr->getObjectProperties().begin(),
                    item_constr->getObjectProperties().end(), child)
                    != item_constr->getObjectProperties().end()))
    {
        childRef = "/" + child->getName();
    }
    else
        cerr << "sdfData::generateReferenceString " + child->getName()
        + " does not belong to " + this->getName()
        + " but references it as parent" << endl;

    if (parent)
        return parent->generateReferenceString(this, import) + childRef;
    else if (parentFile)
        return parentFile->generateReferenceString(this, import) + childRef;
    else
        cerr << "sdfData::generateReferenceString(): sdfData object "
                + this->getName() + " has no assigned parent" << endl;
    return "";

//    else
//    {
//        sdfData *d = dynamic_cast<sdfData*>(parent);
//        sdfObject *o = dynamic_cast<sdfObject*>(parent);
//        sdfAction *a = dynamic_cast<sdfAction*>(parent);
//        sdfEvent *e = dynamic_cast<sdfEvent*>(parent);
//
//        // if the parent is an item constraint data element
//        if (d && !o && !a && !e && this->isItemConstr())
//            return d->generateReferenceString();
//        // if the parent is another sdfData object
//        else if (d && !o  && !a && !e)
//            return parent->generateReferenceString()
//                    + "/" + this->getName();
//
//        // else if this is the input data of an sdfAction object
//        else if (a && a->getInputData() == this)
//            return parent->generateReferenceString()
//                    + "/sdfInputData";
//
//        // else if this is the output data of an sdfAction object
//        // OR if this is the output data of an sdfEvent object
//        else if ((a && a->getOutputData() == this)
//                || (e && e->getOutputData() == this))
//            return parent->generateReferenceString()
//                    + "/sdfOutputData";
//
//        else if (parentFile)
//            return parentFile->generateReferenceString() + "sdfData/"
//                    + this->getName();
//
//        else if (parent)
//            return parent->generateReferenceString()
//                    + "/sdfData/" + this->getName();
//    }
    // this should never be reached
    return "";
}

json sdfData::dataToJson(json prefix)
{
    json data;
    data = this->commonToJson(data);

    if (units != "")
        data["unit"] = this->getUnits();
    if (this->getSubtype() == sdf_byte_string)
        data["sdfType"] = "byte-string";
    else if (this->getSubtype() == sdf_unix_time)
            data["sdfType"] = "unix-time";
    if (this->getContentFormat() != "")
        data["contentFormat"] = this->getContentFormat();
    if (this->readableDefined)
        data["readable"] = this->getReadable();
    if (this->writableDefined)
        data["writable"] = this->getWritable();
    if (this->observableDefined)
        data["observable"] = this->getObservable();
    if (this->nullableDefined)
        data["nullable"] = this->getNullable();
    if (!this->sdfChoice.empty())
    {
        for (sdfData *i : sdfChoice)
        {
            if (i->getSimpType() == json_type_undef)
                i->setType(simpleType);

            json tmpJson;
            data["sdfChoice"][i->getName()]
                            = i->dataToJson(tmpJson)["sdfData"][i->getName()];
        }
        this->setType(json_type_undef);
    }

    // TODO: scaleMinimum and scaleMaximum (where do they go?)
    // TODO: exclusiveMinimum and exclusiveMaximum (bool and float)

    if (constDefined)
    {
        if (!isnan(this->getConstantNumber()))
            data["const"] = this->getConstantNumber();
        else if (this->getConstantString() != "")
            data["const"] = this->getConstantString();
        else if (constBoolDefined)
            data["const"] = this->getConstantBool();
        else if (constIntDefined)
            data["const"] = this->getConstantInt();
        else if (!this->getConstantNumberArray().empty())
            data["const"] = this->getConstantNumberArray();
        else if (!this->getConstantStringArray().empty())
            data["const"] = this->getConstantStringArray();
        else if (!this->getConstantBoolArray().empty())
            data["const"] = this->getConstantBoolArray();
        else if (!this->getConstantIntArray().empty())
            data["const"] = this->getConstantIntArray();
        // TODO: is there even a constant object?
        else if (this->getConstantObject())
        {
            json tmpJson;
            data["const"]
                 = this->getConstantObject()->dataToJson(tmpJson)["sdfData"];
        }
    }
    if (defaultDefined)
    {
        if (!isnan(this->getDefaultNumber()))
            data["default"] = this->getDefaultNumber();
        else if (this->getDefaultString() != "")
            data["default"] = this->getDefaultString();
        else if (defaultBoolDefined)
            data["default"] = this->getDefaultBool();
        else if (defaultIntDefined)
            data["default"] = this->getDefaultInt();
        else if (!this->getDefaultNumberArray().empty())
            data["default"] = this->getDefaultNumberArray();
        else if (!this->getDefaultStringArray().empty())
            data["default"] = this->getDefaultStringArray();
        else if (!this->getDefaultBoolArray().empty())
            data["default"] = this->getDefaultBoolArray();
        else if (!this->getDefaultIntArray().empty())
            data["default"] = this->getDefaultIntArray();
        // TODO: is there even a default object?
        else if (this->getDefaultObject())
        {
            json tmpJson;
            data["default"]
                 = this->getDefaultObject()->dataToJson(tmpJson)["sdfData"];
        }
    }
    if (!isnan(this->getMinimum()))
        data["minimum"] = this->getMinimum();
    if (!isnan(this->getMaximum()))
        data["maximum"] = this->getMaximum();
    if (minIntSet)
        data["minimum"] = this->getMinInt();
    if (maxIntSet)
        data["maximum"] = this->getMaxInt();
    if (!isnan(this->getMultipleOf()))
        data["multipleOf"] = this->getMultipleOf();
    if (!this->getEnumString().empty())
        data["enum"] = this->getEnumString();
    if (!isnan(this->getMinLength()))
        data["minLength"] = this->getMinLength();
    if (!isnan(this->getMaxLength()))
        data["maxLength"] = this->getMaxLength();
    if (this->getPattern() != "")
        data["pattern"] = this->getPattern();
    if (!this->getEnumString().empty())
        data["enum"] = this->getEnumString();
    if (!this->getFormat() == json_format_undef)
        data["format"] = this->getFormat();
    if (!isnan(this->getMinItems()))
        data["minItems"] = this->getMinItems();
    if (!isnan(this->getMaxItems()))
        data["maxItems"] = this->getMaxItems();
    //if (this->getUniqueItems())
    if (this->uniqueItemsDefined)
        data["uniqueItems"] = this->getUniqueItems();
    if (this->item_constr != NULL)
    {
        json tmpJson;
        data["items"] = item_constr->dataToJson(tmpJson)
             ["sdfData"][item_constr->getName()];
    }
    for (sdfData *i : this->getObjectProperties())
    {
        json tmpJson;
        data["properties"][i->getName()]
                 = i->dataToJson(tmpJson)["sdfData"][i->getName()];
    }
    json tmpJson({});
    if (simpleType == json_object && objectProperties.empty())
        data["properties"] = tmpJson;

    if (!requiredObjectProperties.empty())
        data["required"] = requiredObjectProperties;

    if (simpleType != json_type_undef && !this->getReference())
        data["type"] = jsonDTypeToString(simpleType);//derType;

    sdfData *parent = dynamic_cast<sdfData*>(this->getParentCommon());
    if (simpleType == json_integer
            || (parent && parent->getSimpType() == json_integer))
    {
        if (!isnan(this->getMinimum()))
            data["minimum"] = (int)this->getMinimum();
        if (!isnan(this->getMaximum()))
            data["maximum"] = (int)this->getMaximum();

        if (minIntSet)
            data["minimum"] = this->getMinInt();
        if (maxIntSet)
            data["maximum"] = this->getMaxInt();
    }

    prefix["sdfData"][this->getName()] = data;

    return prefix;
}

sdfEvent::sdfEvent(string _name, string _description, sdfCommon *_reference,
        vector<sdfCommon*> _required, sdfObject *_parentObject,
        sdfData* _outputData, vector<sdfData*> _datatypes)
            : sdfObjectElement(_name, _description, _reference, _required),
              sdfCommon(_name, _description, _reference, _required),
              outputData(_outputData), datatypes(_datatypes)
{
    this->setParentObject(_parentObject);
}

sdfEvent::~sdfEvent()
{
    delete outputData;
    outputData = NULL;

    for (int i = 0; i < datatypes.size(); i++)
    {
        delete datatypes[i];
        datatypes[i] = NULL;
    }
    datatypes.clear();

    //delete[] datatypes.data();
}

void sdfEvent::setOutputData(sdfData *outputData)
{
    this->outputData = outputData;
    if (outputData)
        this->outputData->setParentCommon(this);
    else
        cerr << "sdfEvent::setOutputData: empty output data" << endl;
}

void sdfEvent::addDatatype(sdfData *datatype)
{
    this->datatypes.push_back(datatype);
    datatype->setParentCommon((sdfCommon*)this);
}

vector<sdfData*> sdfEvent::getDatatypes()
{
    return this->datatypes;
}

sdfData* sdfEvent::getOutputData()
{
    return this->outputData;
}

//bool sdfEvent::hasChild(sdfCommon *child) const
//{
//    if (outputData && (outputData == child
//            || outputData->getItemConstr() == child
//            || find(outputData->getObjectProperties().begin(),
//                    outputData->getObjectProperties().end(), child)
//                    != outputData->getObjectProperties().end()))
//        return true;
//
//    else if (find(datatypes.begin(), datatypes.end(), child) != datatypes.end())
//        return true;
//
//    return false;
//}

string sdfEvent::generateReferenceString(sdfCommon *child, bool import)
{
    if (!child)
        return this->sdfCommon::generateReferenceString(import);

    string childRef = "";
    if (outputData == child)
    {
        childRef = "/sdfOutputData";
    }
    else if (outputData)
    {
        vector<sdfData*> outOPs = outputData->getObjectProperties();
        if (outputData->getItemConstr() == child
                || find(outOPs.begin(), outOPs.end(), child) != outOPs.end())
            childRef = "/sdfOutputData/" + child->getName();
    }
    else if (find(datatypes.begin(), datatypes.end(), child) != datatypes.end())
    {
        childRef = "/sdfData/" + child->getName();
    }
    else
    {
        cerr << "sdfEvent::generateReferenceString " + child->getName()
        + " does not belong to " + this->getName()
        + " but references it as parent" << endl;
    }

    if (this->getParent())
    {
        return this->getParent()->generateReferenceString(this, import)
                + childRef;
    }
    else if (this->getParentFile())
    {
        return this->getParentFile()->generateReferenceString(this, import)
                + childRef;
    }
    else
    {
        cerr << "Event " + this->getName() + " has no assigned parent" << endl;
    }

    return "";
    /*
    return this->sdfObjectElement::generateReferenceString()
        + "sdfEvent/" + this->getName();*/
}

json sdfEvent::eventToJson(json prefix)
{
    prefix["sdfEvent"][this->getName()]
                     = this->commonToJson(prefix["sdfEvent"][this->getName()]);
    for (sdfData *i : this->getDatatypes())
    {
        prefix["sdfEvent"][this->getName()]
                     = i->dataToJson(prefix["sdfEvent"][this->getName()]);
    }
    if (outputData != NULL)
    {
        json tmp;
        prefix["sdfEvent"][this->getName()]["sdfOutputData"]
          = outputData->dataToJson(tmp)["sdfData"][outputData->getName()];
        //prefix["sdfEvent"][this->getLabel()]["sdfOutputData"][i->getLabel()]["sdfRef"]
        //             = i->generateReferenceString();
    }
    return prefix;
}

void sdfAction::setInputData(sdfData *inputData)
{
    this->inputData = inputData;
    if (inputData)
        this->inputData->setParentCommon(this);
    else
        cerr << "sdfAction::setInputData: empty input data" << endl;
}

void sdfAction::addRequiredInputData(sdfData *requiredInputData)
{
    this->requiredInputData.push_back(requiredInputData);
}

void sdfAction::setOutputData(sdfData *outputData)
{
    this->outputData = outputData;
    if (outputData)
        this->outputData->setParentCommon(this);
    else
        cerr << "sdfAction::setOutputData: empty output data" << endl;
}

void sdfAction::addDatatype(sdfData *datatype)
{
    this->datatypes.push_back(datatype);
    datatype->setParentCommon((sdfCommon*)this);
}

sdfAction::sdfAction(string _name, string _description, sdfCommon *_reference,
        vector<sdfCommon*> _required, sdfObject *_parentObject,
        sdfData* _inputData, vector<sdfData*> _requiredInputData,
        sdfData* _outputData, vector<sdfData*> _datatypes)
            : sdfCommon(_name, _description, _reference, _required),
              inputData(_inputData), requiredInputData(_requiredInputData),
              outputData(_outputData), datatypes(_datatypes)
{
    this->setParentObject(_parentObject);
}

sdfAction::~sdfAction()
{
    delete inputData;
    inputData = NULL;

    for (int i = 0; i < requiredInputData.size(); i++)
    {
        delete requiredInputData[i];
        requiredInputData[i] = NULL;
    }
    requiredInputData.clear();

    for (int i = 0; i < datatypes.size(); i++)
    {
        delete datatypes[i];
        datatypes[i] = NULL;
    }
    datatypes.clear();

    delete outputData;
    outputData = NULL;

    //delete[] requiredInputData.data();
    //delete[] datatypes.data();
}

sdfData* sdfAction::getInputData()
{
    return this->inputData;
}

vector<sdfData*> sdfAction::getRequiredInputData()
{
    return this->requiredInputData;
}

sdfData* sdfAction::getOutputData()
{
    return this->outputData;
}

vector<sdfData*> sdfAction::getDatatypes()
{
    return this->datatypes;
}

//bool sdfAction::hasChild(sdfCommon *child) const
//{
//    if (inputData && (inputData == child
//            || inputData->getItemConstr() == child
//            || find(inputData->getObjectProperties().begin(),
//                    inputData->getObjectProperties().end(), child)
//                    != inputData->getObjectProperties().end()))
//        return true;
//
//    else if (outputData && (outputData == child
//            || outputData->getItemConstr() == child
//            || find(outputData->getObjectProperties().begin(),
//                    outputData->getObjectProperties().end(), child)
//                    != outputData->getObjectProperties().end()))
//        return true;
//
//    else if (find(datatypes.begin(), datatypes.end(), child) != datatypes.end())
//        return true;
//
//    return false;
//}

string sdfAction::generateReferenceString(sdfCommon *child, bool import)
{
    if (!child)
        return this->sdfCommon::generateReferenceString(import);

    string childRef = "";
    if (inputData == child)
        childRef = "/sdfInputData";
    else if (inputData)
    {
        vector<sdfData*> inOPs = inputData->getObjectProperties();
        if (inputData->getItemConstr() == child
                || find(inOPs.begin(), inOPs.end(), child) != inOPs.end())
            childRef = "/sdfInputData/" + child->getName();
    }
    else if (outputData == child)
        childRef = "/sdfOutputData";
    else if (outputData)
    {
        vector<sdfData*> outOPs = outputData->getObjectProperties();
        if (outputData->getItemConstr() == child
                || find(outOPs.begin(), outOPs.end(), child) != outOPs.end())
            childRef = "/sdfOutputData/" + child->getName();
    }
    else if (find(datatypes.begin(), datatypes.end(), child) != datatypes.end())
    {
        childRef = "/sdfData/" + child->getName();
    }
    else
        cerr << "sdfAction::generateReferenceString " + child->getName()
        + " does not belong to " + this->getName()
        + " but references it as parent" << endl;

    if (this->getParent())
        return this->getParent()->generateReferenceString(this, import)
                + childRef;

    else if (this->getParentFile())
        return this->getParentFile()->generateReferenceString(this, import)
                + childRef;

    else
    {
        cerr << "Action " + this->getName() + " has no assigned parent" << endl;
        return "";
    }
    /*
    return this->sdfObjectElement::generateReferenceString()
        + "sdfAction/" + this->getName();
        */
}

json sdfAction::actionToJson(json prefix)
{
    prefix["sdfAction"][this->getName()]
            = this->commonToJson(prefix["sdfAction"][this->getName()]);

    for (sdfData *i : this->getDatatypes())
    {
        prefix["sdfAction"][this->getName()]
                     = i->dataToJson(prefix["sdfAction"][this->getName()]);
    }/*
    for (sdfData *i : this->getInputData())
    {
        prefix["sdfAction"][this->getLabel()]["sdfInputData"][i->getLabel()]["sdfRef"]
                             = i->generateReferenceString();
    }*/
    if (inputData != NULL)
    {
        json tmp;
        prefix["sdfAction"][this->getName()]["sdfInputData"]
                 = inputData->dataToJson(tmp)["sdfData"][inputData->getName()];
    }
    if (outputData != NULL)
    {
        json tmp;
        prefix["sdfAction"][this->getName()]["sdfOutputData"]
               = outputData->dataToJson(tmp)["sdfData"][outputData->getName()];
    }

    return prefix;
}

sdfProperty::sdfProperty(string _name, string _description,
        jsonDataType _type, sdfCommon *_reference,
        vector<sdfCommon*> _required, sdfObject *_parentObject)
        : sdfData(_name, _description, _type, _reference, _required),
          sdfObjectElement(_name, _description, _reference, _required),
          sdfCommon(_name, _description, _reference, _required)
{
    this->setParentObject(_parentObject);
    this->setParentCommon(NULL);
}

sdfProperty::sdfProperty(sdfData &data)
: sdfData(data),
  sdfObjectElement(data.getName(), data.getDescription(), data.getReference(),
          data.getRequired()),
  sdfCommon(data.getName(), data.getDescription(), data.getReference(),
          data.getRequired())
{
    this->setParentObject(NULL);
    this->setParentCommon(NULL);
    vector<sdfData*> d = this->getObjectProperties();
    for (int i = 0; i < d.size(); i++)
        d.at(i)->setParentCommon((sdfCommon*)this);

    d = this->getChoice();
    for (int i = 0; i < d.size(); i++)
        d.at(i)->setParentCommon((sdfCommon*)this);

    sdfData *ic = this->getItemConstr();
    if (ic)
    {
        ic->setParentCommon((sdfCommon*)this);
//        for (int i = 0; i < ic->getObjectProperties().size(); i++)
//            ic->getObjectProperties().at(i)->setParentCommon((sdfCommon*)this);
    }
}

//bool sdfProperty::hasChild(sdfCommon *child) const
//{
//    return this->sdfData::hasChild(child);
//}

string sdfProperty::generateReferenceString(sdfCommon *child, bool import)
{
    /*return this->sdfObjectElement::generateReferenceString()
        + "sdfProperty/" + this->getName();*/

    return this->sdfData::generateReferenceString(child, import);
}

json sdfProperty::propertyToJson(json prefix)
{
    json output = prefix;
    output =  this->dataToJson(output);
    prefix["sdfProperty"][this->getName()]
                          = output["sdfData"][this->getName()];
    return prefix;
}

sdfObject::sdfObject(string _name, string _description, sdfCommon *_reference,
        vector<sdfCommon*> _required, vector<sdfProperty*> _properties,
        vector<sdfAction*> _actions, vector<sdfEvent*> _events,
        vector<sdfData*> _datatypes, sdfThing *_parentThing)
            : sdfCommon(_name, _description, _reference, _required),
              properties(_properties), actions(_actions), events(_events),
              datatypes(_datatypes)
{
    this->ns = NULL;
    this->info = NULL;
    this->setParentThing(_parentThing);
}

sdfObject::~sdfObject()
{
    for (int i = 0; i < properties.size(); i++)
    {
        delete properties[i];
        properties[i] = NULL;
    }
    properties.clear();

    for (int i = 0; i < actions.size(); i++)
    {
        delete actions[i];
        actions[i] = NULL;
    }
    actions.clear();

    for (int i = 0; i < events.size(); i++)
    {
        delete events[i];
        events[i] = NULL;
    }
    events.clear();

    for (int i = 0; i < datatypes.size(); i++)
    {
        delete datatypes[i];
        datatypes[i] = NULL;
    }
    datatypes.clear();

    /*delete[] properties.data();
    delete[] actions.data();
    delete[] events.data();
    delete[] datatypes.data();*/

    delete ns;
    ns = NULL;
    delete info;
    info = NULL;
    // parent cannot be deleted here because that would cause a deletion loop
    //delete parent;
    parent = NULL;
}

void sdfObject::setInfo(sdfInfoBlock *_info)
{
    this->info = _info;
}

void sdfObject::setNamespace(sdfNamespaceSection *_ns)
{
    this->ns = _ns;
}

void sdfObject::addProperty(sdfProperty *property)
{
    this->properties.push_back(property);
    property->setParentObject(this);
    property->setParentCommon((sdfCommon*)this);
}

void sdfObject::addAction(sdfAction *action)
{
    this->actions.push_back(action);
    action->setParentObject(this);
}

void sdfObject::addEvent(sdfEvent *event)
{
    this->events.push_back(event);
    event->setParentObject(this);
}

void sdfObject::addDatatype(sdfData *datatype)
{
    this->datatypes.push_back(datatype);
    datatype->setParentCommon((sdfCommon*)this);
    //datatype->setParentCommon(this);

}

sdfInfoBlock* sdfObject::getInfo()
{
    return this->info;
}

sdfNamespaceSection* sdfObject::getNamespace()
{
    return this->ns;
}

vector<sdfProperty*> sdfObject::getProperties()
{
    return this->properties;
}

vector<sdfAction*> sdfObject::getActions()
{
    return this->actions;
}

vector<sdfEvent*> sdfObject::getEvents()
{
    return this->events;
}

vector<sdfData*> sdfObject::getDatatypes()
{
    return this->datatypes;
}

sdfThing* sdfObject::getParentThing() const
{
    return parent;
}

void sdfObject::setParentThing(sdfThing *parentThing)
{
    this->parent = parentThing;
    // TODO: also add this to parentThings object list?
    // make sure the object is in the list only once
    // parentThing->addObject(this);
}

json sdfObject::objectToJson(json prefix, bool print_info_namespace)
{
    // print info if specified by print_info
    if (print_info_namespace)
    {
        if (this->info != NULL)
            prefix = this->info->infoToJson(prefix);
        if (this->ns != NULL)
            prefix = this->ns->namespaceToJson(prefix);
    }

    prefix["sdfObject"][this->getName()]
                    = this->commonToJson(prefix["sdfObject"][this->getName()]);

    for (sdfData *i : this->getDatatypes())
    {
        prefix["sdfObject"][this->getName()]
                    = i->dataToJson(prefix["sdfObject"][this->getName()]);
    }
    for (sdfProperty *i : this->getProperties())
    {
        prefix["sdfObject"][this->getName()]
                    = i->propertyToJson(prefix["sdfObject"][this->getName()]);
    }
    for (sdfAction *i : this->getActions())
    {
        prefix["sdfObject"][this->getName()]
                    = i->actionToJson(prefix["sdfObject"][this->getName()]);
    }
    for (sdfEvent *i : this->getEvents())
    {
        prefix["sdfObject"][this->getName()]
                    = i->eventToJson(prefix["sdfObject"][this->getName()]);
    }

    return prefix;
}

string sdfObject::objectToString(bool print_info_namespace)
{
    json json_output;
    return this->objectToJson(
            json_output, print_info_namespace).dump(INDENT_WIDTH);
}

void sdfObject::objectToFile(string path)
{
    std::ofstream output(path);
    if (output)
    {
        output << this->objectToString(true) << std::endl;
        output.close();
    }
    else
        cerr << "Error opening file" << endl;

    validateFile(path);
}

string sdfObject::generateReferenceString(sdfCommon *child, bool import)
{
    if (!child)
        return this->sdfCommon::generateReferenceString(import);

    string childRef = "";
    if (find(datatypes.begin(), datatypes.end(), child) != datatypes.end())
        childRef = "/sdfData/";

    else if (find(properties.begin(), properties.end(), child) !=
            properties.end())
        childRef = "/sdfProperty/";

    else if (find(actions.begin(), actions.end(), child) != actions.end())
        childRef = "/sdfAction/";

    else if (find(events.begin(), events.end(), child) != events.end())
        childRef = "/sdfEvent/";

    if (parent)
        return parent->generateReferenceString(this, import) + childRef
                + child->getName();

    else if (this->getParentFile())
        return this->getParentFile()->generateReferenceString(this, import)
                + childRef + child->getName();

    else if (import && this->getNamespace()
            && this->getNamespace()->getDefaultNamespace() != "")
        return this->getNamespace()->getDefaultNamespace() + ":/sdfObject/"
                + this->getName() + childRef + child->getName();
    else
        return "#/sdfObject/" + this->getName() + childRef + child->getName();

    /*
    if (this->parent != NULL)
        return this->parent->generateReferenceString() + "/sdfObject/"
                + this->getName();

    else if (this->getParentFile())
        return this->getParentFile()->generateReferenceString()
                + "sdfObject/" + this->getName();

    else
        return "#/sdfObject/" + this->getName();
        */
}

sdfThing::sdfThing(string _name, string _description, sdfCommon *_reference,
        vector<sdfCommon*> _required, vector<sdfThing*> _things,
        vector<sdfObject*> _objects, sdfThing *_parentThing)
            : sdfCommon(_name, _description, _reference, _required),
              childThings(_things)//, childObjects(_objects)
{
    this->ns = NULL;
    this->info = NULL;
    this->setParentThing(_parentThing);
}

sdfThing::~sdfThing()
{
    for (int i = 0; i < childThings.size(); i++)
    {
        delete childThings[i];
        childThings[i] = NULL;
    }
    childThings.clear();

    for (int i = 0; i < childObjects.size(); i++)
    {
        delete childObjects[i];
        childObjects[i] = NULL;
    }
    childObjects.clear();

    //delete[] childThings.data();
    //delete[] childObjects.data();

    delete ns;
    ns = NULL;
    delete info;
    info = NULL;

    // parent cannot be deleted because that would create a deletion loop
    //delete parent;
    parent = NULL;
}

void sdfThing::setInfo(sdfInfoBlock *_info)
{
    this->info = _info;
}

void sdfThing::setNamespace(sdfNamespaceSection *_ns)
{
    this->ns = _ns;
}

sdfInfoBlock* sdfThing::getInfo() const
{
    return this->info;
}

sdfNamespaceSection* sdfThing::getNamespace() const
{
    return this->ns;
}

void sdfThing::addThing(sdfThing *thing)
{
    this->childThings.push_back(thing);
    thing->setParentThing(this);
}

void sdfThing::addObject(sdfObject *object)
{
    this->childObjects.push_back(object);
    object->setParentThing(this);
}

vector<sdfThing*> sdfThing::getThings() const
{
    return this->childThings;
}

vector<sdfObject*> sdfThing::getObjects() const
{
    return this->childObjects;
}

json sdfThing::thingToJson(json prefix, bool print_info_namespace)
{
    // print info if specified by print_info
    if (print_info_namespace)
    {
        if (this->info != NULL)
            prefix = this->info->infoToJson(prefix);
        if (this->ns != NULL)
            prefix = this->ns->namespaceToJson(prefix);
    }
    if (this->getName() != "")
        prefix["sdfThing"][this->getName()]
                = this->commonToJson(prefix["sdfThing"][this->getName()]);
    for (sdfThing *i : this->childThings)
    {
        prefix["sdfThing"][this->getName()] = i->thingToJson(
                prefix["sdfThing"][this->getName()], false);
    }

    for (sdfObject *i : this->childObjects)
    {
        prefix["sdfThing"][this->getName()] = i->objectToJson(
                prefix["sdfThing"][this->getName()], false);
    }
    return prefix;
}

string sdfThing::thingToString(bool print_info_namespace)
{
    json json_output;
    return this->thingToJson(
            json_output, print_info_namespace).dump(INDENT_WIDTH);
}

void sdfThing::thingToFile(string path)
{
    ofstream output(path);
    if (output)
    {
        output << this->thingToString() << endl;
        output.close();
    }
    else
        cerr << "Error opening file" << endl;

    validateFile(path);
}

sdfThing* sdfThing::getParentThing() const
{
    return parent;
}

void sdfThing::setParentThing(sdfThing *parentThing)
{
    this->parent = parentThing;
}

string sdfThing::generateReferenceString(sdfCommon *child, bool import)
{
    if (!child)
        return this->sdfCommon::generateReferenceString(import);

    string childRef = "";
    if (find(childThings.begin(), childThings.end(), child)
            != childThings.end())
        childRef = "/sdfThing/";

    else if (find(childObjects.begin(), childObjects.end(), child)
            != childObjects.end())
        childRef = "/sdfObject/";

    else
        cerr << "sdfThing::generateReferenceString " + child->getName()
        + " does not belong to " + this->getName()
        + " but references it as parent" << endl;

    if (parent)
        return parent->generateReferenceString(this, import) + childRef
                + child->getName();

    else if (this->getParentFile())
    {
        return this->getParentFile()->generateReferenceString(this, import)
                + childRef + child->getName();
    }
    else if (import && this->getNamespace()
            && this->getNamespace()->getDefaultNamespace() != "")
            return this->getNamespace()->getDefaultNamespace() + ":/sdfThing/"
                    + this->getName() + childRef + child->getName();

    else
        return "#/sdfThing/" + this->getName() + childRef + child->getName();
    /*
    if (this->parent != NULL)
        return this->parent->generateReferenceString() + "/sdfThing/"
                + this->getName();

    else if (this->getParentFile())
        return this->getParentFile()->generateReferenceString() +
                "sdfThing/" + this->getName();

    else
        return "#/sdfThing/" + this->getName();*/
}

void sdfCommon::jsonToCommon(json input)
{
    for (auto it : input.items())
    {
        //cout << "jsonToCommon: " << it.key() << endl;
        if (it.key() == "label")
            this->setLabel(correctValue(it.value()));
        else if (it.key() == "description")
            this->setDescription(it.value());
        else if (it.key() == "sdfRef")
        {
            unassignedRefs.push_back(tuple<string, sdfCommon*>{
                correctValue(it.value()), this});
            //cout << correctValue(it.value())+" "+this->getName() << endl;
        }
        else if (it.key() == "sdfRequired")
            for (auto jt : it.value())
                unassignedReqs.push_back(tuple<string, sdfCommon*>{
                    correctValue(jt), this});
    }
    // check if this is the item constraint of an sdfData element
    sdfData *data = this->getThisAsSdfData();
    if (!data || !data->isItemConstr())
    {
        // if not, add to existing definitions
        existingDefinitons[this->generateReferenceString()] = this;
//        cout << "!!!jsonToCommon: " << this->generateReferenceString() <<" "
//            << this->getName()<< endl;
    }
}

sdfData* sdfData::jsonToData(json input)
{
    this->jsonToCommon(input);
    for (json::iterator it = input.begin(); it != input.end(); ++it)
    {
        if (it.key() == "type" && !it.value().empty())
        {
            this->setType((string)input["type"]);
            // if type is number, maybe the const and default values have to
            // be reassigned
            if (simpleType == json_number)
            {
                if (defaultIntDefined)
                {
                    defaultNumber = defaultInt;
                    defaultIntDefined = false;
                }
                if (constIntDefined)
                {
                    constantNumber = constantInt;
                    constIntDefined = false;
                }
            }
        }
        else if (it.key() == "enum" && !it.value().empty())
        {
            if (it.value().is_array())
            {
                for (auto i : it.value())
                {
                    if (i.is_string())
                        this->enumString.push_back(i);
                    /*if (i.is_number_integer())
                        this->enumInt.push_back(i);
                    if (i.is_number())
                        this->enumNumber.push_back(i);
                    else if (i.is_boolean())
                        this->enumBool.push_back(i);
                    else if (i.is_array())
                        ; // fix this?*/
                }
            }
        }
        else if (it.key() == "sdfChoice" && !it.value().empty())
        {
            for (json::iterator jt = it.value().begin(); jt != it.value().end();
                    ++jt)
            {
                sdfData *choice = new sdfData();
                choice->setName(correctValue(jt.key()));
                this->addChoice(choice);
                choice->jsonToData(jt.value());
                //this->addChoice(choice->jsonToData(jt.value()));
            }
        }
        else if (it.key() == "required" && !it.value().empty())
        {
            if (it.value().is_array() && it.value()[0].is_string())
            {
                for (auto i : it.value())
                    this->addRequiredObjectProperty((string)i);
            }
            else
                cerr << "jsonToData(): field \'required\' could not be parsed"
                << endl;
        }
        else if (it.key() == "properties" && !it.value().empty())
        {
            for (json::iterator jt = it.value().begin(); jt != it.value().end();
                                ++jt)
            {
                //cout << jt.key() << endl;
                sdfData *objectProperty = new sdfData();
                objectProperty->setName(correctValue(jt.key()));
                this->addObjectProperty(objectProperty);
                objectProperty->jsonToData(jt.value());
                //objectProperty->setParentCommon(this);
                //this->addObjectProperty(objectProperty->jsonToData(jt.value()));
            }
        }
        if (it.key() == "const" && !it.value().empty())
        {
            this->constDefined = true;
            if (it.value().is_number_integer())
            {
                this->constantInt = it.value();
                this->constIntDefined = true;
            }
            else if (it.value().is_number())
                this->constantNumber = it.value();
            else if (it.value().is_string())
                this->constantString = it.value();
            else if (it.value().is_boolean())
            {
                this->constantBool = it.value();
                this->constBoolDefined = true;
            }
            else if (it.value().is_array())
            {
                for (json::iterator jt = it.value().begin();
                        jt != it.value().end(); ++jt)
                {
                    if (jt.value().is_number_integer()
                            && (simpleType == json_integer
                                    || simpleType == json_type_undef))
                        this->constantIntArray.push_back(jt.value());

                    else if (jt.value().is_number())
                        this->constantNumberArray.push_back(jt.value());

                    else if (jt.value().is_string())
                        this->constantStringArray.push_back(jt.value());

                    else if (jt.value().is_boolean())
                        this->constantBoolArray.push_back(jt.value());
                }
            }
        }
        else if (it.key() == "default" && !it.value().empty())
        {
            this->defaultDefined = true;
            if (it.value().is_number_integer()
                    && (simpleType == json_integer
                            || simpleType == json_type_undef))
            {
                this->defaultInt = it.value();
                this->defaultIntDefined = true;
            }
            else if (it.value().is_number())
                this->defaultNumber = it.value();
            else if (it.value().is_string())
                this->defaultString = it.value();
            else if (it.value().is_boolean())
            {
                this->defaultBool = it.value();
                this->defaultBoolDefined = true;
            }
            else if (it.value().is_array())
            {
                for (json::iterator jt = it.value().begin();
                        jt != it.value().end(); ++jt)
                {
                    if (jt.value().is_number_integer()
                            && (simpleType == json_integer
                                    || simpleType == json_type_undef))
                    {
                        this->defaultIntArray.push_back(jt.value());
                    }

                    else if (jt.value().is_number())
                        this->defaultNumberArray.push_back(jt.value());

                    else if (jt.value().is_string())
                        this->defaultStringArray.push_back(jt.value());

                    else if (jt.value().is_boolean())
                        this->defaultBoolArray.push_back(jt.value());
                }
            }
        }
        else if (it.key() == "minimum" && !it.value().empty())
        {
            if (it.value().is_number_integer()
                    && (simpleType == json_integer
                            || simpleType == json_type_undef))
            {
                this->setMinInt(it.value());
            }
            else
                this->minimum = it.value();
        }
        else if (it.key() == "maximum" && !it.value().empty())
        {
            if (it.value().is_number_integer()
                    && (simpleType == json_integer
                            || simpleType == json_type_undef))
            {
                this->setMaxInt(it.value());
            }
            else
                this->maximum = it.value();
        }
        else if (it.key() == "exclusiveMinimum" && !it.value().empty())
        {
            if (it.value().is_boolean())
                this->exclusiveMinimum_bool = it.value();
            else if (it.value().is_number())
                this->exclusiveMinimum_number = it.value();
        }
        else if (it.key() == "exclusiveMaximum" && !it.value().empty())
        {
            if (it.value().is_boolean())
                this->exclusiveMaximum_bool = it.value();
            else if (it.value().is_number())
                this->exclusiveMaximum_number = it.value();
        }
        else if (it.key() == "multipleOf" && !it.value().empty())
        {
            this->multipleOf = it.value();
        }
        else if (it.key() == "minLength" && !it.value().empty())
        {
            this->minLength = it.value();
        }
        else if (it.key() == "maxLength" && !it.value().empty())
        {
            this->maxLength = it.value();
        }
        else if (it.key() == "pattern" && !it.value().empty())
        {
            this->pattern = it.value();
        }
        else if (it.key() == "format" && !it.value().empty())
        {
            if (it.value() == "date-time")
                this->contentFormat = json_date_time;
            else if (it.value() == "date")
                this->contentFormat = json_date;
            else if (it.value() == "time")
                this->contentFormat = json_time;
            else if (it.value() == "uri")
                this->contentFormat = json_uri;
            else if (it.value() == "uri-reference")
                this->contentFormat = json_uri_reference;
            else if (it.value() == "uuid")
                this->contentFormat = json_uuid;
            else
                this->contentFormat = json_format_undef;
        }
        else if (it.key() == "minItems" && !it.value().empty())
        {
            this->minItems = it.value();
        }
        else if (it.key() == "maxItems" && !it.value().empty())
        {
            this->maxItems = it.value();
        }
        else if (it.key() == "uniqueItems" && !it.value().empty())
        {
            this->uniqueItems = it.value();
            this->uniqueItemsDefined = true;
        }
        else if (it.key() == "items" && !it.value().empty())
        {
            //this->item_constr = new sdfData();
            //this->item_constr->jsonToData(input["items"]);
            sdfData *itemConstr = new sdfData();
            this->setItemConstr(itemConstr);
            itemConstr->jsonToData(input["items"]);
        }
        // TODO: keep key "units" for older versions?
        else if ((it.key() == "unit" || it.key() == "units")
                && !it.value().empty())
        {
            this->units = it.value();
        }
        else if (it.key() == "scaleMinimum" && !it.value().empty())
        {
            this->scaleMinimum = it.value();
        }
        else if (it.key() == "scaleMaximum" && !it.value().empty())
        {
            this->scaleMaximum = it.value();
        }
        else if (it.key() == "readable" && !it.value().empty())
        {
            this->readable = it.value();
            this->readableDefined = true;
        }
        else if (it.key() == "writable" && !it.value().empty())
        {
            this->writable = it.value();
            this->writableDefined = true;
        }
        else if (it.key() == "observable" && !it.value().empty())
        {
            this->observable = it.value();
            this->observableDefined = true;
        }
        else if (it.key() == "nullable" && !it.value().empty())
        {
            this->nullable = it.value();
            this->nullableDefined = true;
        }
        else if (it.key() == "contentFormat" && !it.value().empty())
        {
            // TODO: ??
        }
        //else if (it.key() == "subtype" && !it.value().empty())
        // TODO: keep "subtype" for older versions?
        else if ((it.key() == "sdfType" || it.key() == "subtype")
                && !it.value().empty())
        {
            if (it.value() == "byte-string")
                this->subtype = sdf_byte_string;
            else if (it.value() == "unix-time")
                this->subtype = sdf_unix_time;
            else
                this->subtype = sdf_subtype_undef;
        }
    }
    //this->jsonToCommon(input);
    return this;
}

sdfEvent* sdfEvent::jsonToEvent(json input)
{
    this->jsonToCommon(input);
    for (auto it : input.items())
    {
        if (it.key() == "sdfOutputData" && !it.value().empty())
        {
            /*
            for (json::iterator jt = it.value().begin(); jt != it.value().end(); ++jt)
            {
                cout << "HELLLO" << jt.key() << endl;
                sdfData *data = new sdfData();
                data->setLabel(correctValue(jt.key()));
                data->jsonToData(input["sdfOutputData"]);
                this->addOutputData(data);
            }*/
            sdfData *data = new sdfData();
            //data->setLabel("");
            this->setOutputData(data);
            data->jsonToData(input["sdfOutputData"]);

        }
        else if (it.key() == "sdfData" && !it.value().empty())
        {
            for (json::iterator jt = it.value().begin(); jt != it.value().end(); ++jt)
            {
                sdfData *childData = new sdfData();
                childData->setName(correctValue(jt.key()));
                this->addDatatype(childData);
                childData->jsonToData(input["sdfData"][jt.key()]);
            }
        }
    }
    //this->jsonToCommon(input);
    return this;
}

sdfAction* sdfAction::jsonToAction(json input)
{
    this->jsonToCommon(input);
    for (json::iterator it = input.begin(); it != input.end(); ++it)
    {
        if (it.key() == "sdfInputData" && !it.value().empty())
        {/*
            for (json::iterator jt = it.value().begin(); jt != it.value().end(); ++jt)
            {
                sdfData *refData = new sdfData();
                this->addInputData(refData);
            }*/
            sdfData *data = new sdfData();
            this->setInputData(data);
            data->jsonToData(input["sdfInputData"]);
        }
        else if (it.key() == "sdfRequiredInputData" && !it.value().empty())
        {
            for (json::iterator jt = it.value().begin(); jt != it.value().end(); ++jt)
            {
                sdfData *refData = new sdfData();
                this->addRequiredInputData(refData);
            }
        }
        else if (it.key() == "sdfOutputData" && !it.value().empty())
        {/*
            for (json::iterator jt = it.value().begin(); jt != it.value().end(); ++jt)
            {
                sdfData *refData = new sdfData();
                this->addOutputData(refData);
            }*/
            sdfData *data = new sdfData();
            this->setOutputData(data);
            data->jsonToData(input["sdfOutputData"]);
        }
        else if (it.key() == "sdfData" && !it.value().empty())
        {
            for (json::iterator jt = it.value().begin(); jt != it.value().end(); ++jt)
            {
                sdfData *childData = new sdfData();
                childData->setName(correctValue(jt.key()));
                this->addDatatype(childData);
                childData->jsonToData(input["sdfData"][jt.key()]);
            }
        }
    }
    //this->jsonToCommon(input);
    return this;
}

sdfProperty* sdfProperty::jsonToProperty(json input)
{
    //this->jsonToCommon(input);
    this->jsonToData(input);
    //this->jsonToCommon(input);
    return this;
}

void sdfNamespaceSection::makeDefinitionsGlobal()
{
    // insert all definitions of this element into the global definitions
    // and add the default prefix to path
    string newRef;
    map<string, sdfCommon*>::iterator it = existingDefinitons.begin();
    for (it = existingDefinitons.begin(); it != existingDefinitons.end(); it++)
    {
        newRef = it->first;
        regex hashSplit ("#(.*)");
        smatch sm;
        if (this->getDefaultNamespace() != "")
        {
            if (regex_match(newRef, sm, hashSplit))
                newRef = this->getDefaultNamespace() + ":" + string(sm[1]);
            if (it->second)
            {
//                cout << newRef << endl;
//                cout << it->second->getName() << endl;
                existingDefinitonsGlobal[newRef] = it->second;
            }
        }
    }
    //existingDefinitons.clear();
    existingDefinitons = {};
}

sdfObject* sdfObject::jsonToObject(json input, bool testForThing)
{
    this->jsonToCommon(input);
    for (json::iterator it = input.begin(); it != input.end(); ++it)
    {
        //cout << "jsonToObject: " << it.key() << endl;
        if (it.key() == "info" && !it.value().empty())
        {
            sdfInfoBlock *info = new sdfInfoBlock();
            this->setInfo(info);
            this->info->jsonToInfo(input["info"]);
        }
        else if (it.key() == "namespace" && !it.value().empty())
        {
            sdfNamespaceSection *ns = new sdfNamespaceSection();
            this->setNamespace(ns);
            this->ns->jsonToNamespace(input);
        }
        // for first level
        else if (it.key() == "sdfObject" && !it.value().empty())
        {
            for (json::iterator jt = it.value().begin();
                    jt != it.value().end(); ++jt)
            {
                this->setName(correctValue(jt.key()));
                this->jsonToObject(input["sdfObject"][jt.key()]);
            }
        }
        else if (it.key() == "sdfData" && !it.value().empty())
        {
            for (json::iterator jt = it.value().begin();
                    jt != it.value().end(); ++jt)
            {
                sdfData *childData = new sdfData();
                this->addDatatype(childData);
                childData->setName(correctValue(jt.key()));
                childData->jsonToData(input["sdfData"][jt.key()]);
            }
        }
        else if (it.key() == "sdfProperty" && !it.value().empty())
        {
            for (json::iterator jt = it.value().begin();
                    jt != it.value().end(); ++jt)
            {
                sdfProperty *childProperty = new sdfProperty();
                this->addProperty(childProperty);
                childProperty->setName(correctValue(jt.key()));
                childProperty->jsonToProperty(input["sdfProperty"][jt.key()]);

                //childProperty->setLabel("hi_oh");//jt.key());
            }
        }
        else if (it.key() == "sdfAction" && !it.value().empty())
        {
            for (json::iterator jt = it.value().begin();
                    jt != it.value().end(); ++jt)
            {
                sdfAction *childAction = new sdfAction();
                this->addAction(childAction);
                childAction->setName(correctValue(jt.key()));
                childAction->jsonToAction(input["sdfAction"][jt.key()]);
            }
        }
        else if (it.key() == "sdfEvent" && !it.value().empty())
        {
            for (json::iterator jt = it.value().begin();
                    jt != it.value().end(); ++jt)
            {
                sdfEvent *childEvent =  new sdfEvent();
                this->addEvent(childEvent);
                // TODO: with label set this way, a label will be printed
                // even though there was just a "title" in the original
                childEvent->setName(correctValue(jt.key()));
                childEvent->jsonToEvent(input["sdfEvent"][jt.key()]);
            }
        }
        else if (it.key() == "sdfThing")
        {
            if (!testForThing)
                cerr << "jsonToObject(): incorrect sdfObject (sdfThing found)"
                    << endl;
            return NULL;
        }
        /*
        else if (it.key() != "label" && it.key() != "description" &&
                it.key() != "sdfRef" && it.key() != "sdfRequired")
        {
            printf("deeper\n");
            this->jsonToObject(input[it.key()]);
        }*/
    }

    // only try to assign refs when this is a top level object
    if (!this->getParentThing() && !this->getParentFile())
    {
        // assign sdfRef and sdfRequired references
        unassignedRefs = assignRefs(unassignedRefs, REF);
        unassignedReqs = assignRefs(unassignedReqs, REQ);

        if (!unassignedRefs.empty() || !unassignedReqs.empty())
            cerr << "There is/are "
            + to_string(unassignedRefs.size()+unassignedReqs.size())
            + " reference(s) left unassigned" << endl;
        else
            cout << "All references resolved" << endl;

        if (isContext)
        {
//            unassignedRefs = {};
//            unassignedReqs = {};

            if (this->getNamespace())
                this->getNamespace()->makeDefinitionsGlobal();
        }
    }
    // jsonToCommon needs to be called twice because of sdfRequired
    // (which cannot be filled before the rest of the object)
    // not anymore (changed way of assigning sdfRef)
    //this->jsonToCommon(input);
    return this;
}

//bool sdfObject::hasChild(sdfCommon *child) const
//{
//    if (find(properties.begin(), properties.end(), child)
//            != properties.end())
//        return true;
//    else if (find(datatypes.begin(), datatypes.end(), child)
//            != datatypes.end())
//        return true;
//    else if (find(actions.begin(), actions.end(), child)
//            != actions.end())
//        return true;
//    else if (find(events.begin(), events.end(), child)
//            != events.end())
//        return true;
//
//    return false;
//}

sdfObject* sdfObject::fileToObject(string path, bool testForThing)
{
    if (!contextLoaded)
        loadContext();

    json json_input;
    ifstream input(path);
    if (input)
    {
        input >>  json_input;
        input.close();
    }
    else
    {
        cerr << "Error opening file" << endl;
        return NULL;
    }
    return this->jsonToObject(json_input, testForThing);
}

//bool sdfThing::hasChild(sdfCommon *child) const
//{
//    if (find(childThings.begin(), childThings.end(), child)
//            != childThings.end())
//        return true;
//    else if (find(childObjects.begin(), childObjects.end(), child)
//            != childObjects.end())
//        return true;
//
//    return false;
//}

sdfThing* sdfThing::jsonToThing(json input, bool nested)
{
    // if we are just loading the context, ignore things that do not
    // have a default namespace and hence do not contribute to the
    // global namespace
    if (false&&isContext && (!this->getParentFile()->getNamespace()
            || this->getParentFile()->getNamespace()->getDefaultNamespace() == ""))
        return NULL;

    this->jsonToCommon(input);

    for (json::iterator it = input.begin(); it != input.end(); ++it)
    {
        //cout << "jsonToThing: " << it.key() << endl;
        if (it.key() == "info" && !it.value().empty())
        {
            sdfInfoBlock *info = new sdfInfoBlock();
            this->setInfo(info);
            this->info->jsonToInfo(input["info"]);
        }
        else if (it.key() == "namespace" && !it.value().empty())
        {
            cout << "OOOOOOOOOOOOOOOOOOOOOOOOOOO"<<endl;
            sdfNamespaceSection *ns = new sdfNamespaceSection();
            this->setNamespace(ns);
            this->ns->jsonToNamespace(input);
        }
        else if (it.key() == "defaultNamespace" && !it.value().empty())
        {
            this->ns->jsonToNamespace(input["namespace"]);
        }
        else if (it.key() == "sdfThing" && !it.value().empty())
        {
            for (json::iterator jt = it.value().begin();
                    jt != it.value().end(); ++jt)
            {
                if (!nested)
                {
                    this->setName(correctValue(jt.key()));
                    this->jsonToThing(input["sdfThing"][jt.key()], true);
                    //this->jsonToNestedThing(input["sdfThing"][jt.key()]);
                }
                else
                {
                    sdfThing *childThing = new sdfThing();
                    childThing->setName(correctValue(jt.key()));
                    this->addThing(childThing);
                    childThing->jsonToThing(input["sdfThing"][jt.key()], true);
                }
            }
        }
        else if (it.key() == "sdfObject" && !it.value().empty())
        {
            for (json::iterator jt = it.value().begin();
                    jt != it.value().end(); ++jt)
            {
                sdfObject *childObject = new sdfObject();
                childObject->setName(correctValue(jt.key()));
                this->addObject(childObject);
                childObject->jsonToObject(input["sdfObject"][jt.key()]);
            }
        }
    }
    // only try to assign refs if this thing is at the top level
    if (!nested || (!this->getParentThing() && !this->getParentFile()))
    {
        // assign sdfRef and sdfRequired references
        unassignedRefs = assignRefs(unassignedRefs, REF);
        unassignedReqs = assignRefs(unassignedReqs, REQ);

        if (!unassignedRefs.empty() || !unassignedReqs.empty())
            cerr << "There is/are "
            + to_string(unassignedRefs.size()+unassignedReqs.size())
            + " reference(s) left unassigned" << endl;
        else
            cout << "All references resolved" << endl;

        if (isContext)
        {
//            unassignedRefs = {};
//            unassignedReqs = {};

            if (this->getNamespace())
                this->getNamespace()->makeDefinitionsGlobal();
        }
    }
    //this->jsonToCommon(input);
    return this;
}

sdfThing* sdfThing::fileToThing(string path)
{
    if (!contextLoaded)
        loadContext();

    json json_input;
    ifstream input(path);
    if (input)
    {
        input >>  json_input;
        input.close();
    }
    else
    {
        cerr << "Error opening file" << endl;
        return NULL;
    }
    return this->jsonToThing(json_input);
}
/*
void sdfCommon::setParentCommon(sdfCommon *parentCommon)
{
    //cout << parentCommon->getLabel() << endl;
    this->parent = parentCommon;
}

sdfCommon* sdfCommon::getParentCommon()
{
    return this->parent;
}
*/
void sdfCommon::setDescription(string dsc)
{
    this->description = dsc;
}

void sdfData::setSimpType(jsonDataType _type)
{
    this->simpleType = _type;
}

void sdfData::setDerType(string _type)
{
    //this->simpleType = stringToJsonDType(_type);
    this->derType = _type;
}

void sdfData::setType(jsonDataType _type)
{
    this->derType = jsonDTypeToString(_type);
    this->simpleType = _type;
}

void sdfData::setType(string _type)
{
    this->derType = _type;
    this->simpleType = stringToJsonDType(_type);
}

bool sdfData::getReadable()
{
    return this->readable;
}

bool sdfData::getWritable()
{
    return this->writable;
}

void sdfData::setMaxItems(float maxItems)
{
    this->maxItems = maxItems;
}

void sdfData::setMinItems(float minItems)
{
    this->minItems = minItems;
}

void sdfData::setPattern(string pattern)
{
    this->pattern = pattern;
}

void sdfData::setConstantBool(bool constantBool)
{
    this->constantBool = constantBool;
    this->constDefined = true;
    this->constBoolDefined = true;
}

void sdfData::setConstantInt(int64_t _constantInt)
{
    this->constantInt = _constantInt;
    this->constDefined = true;
    this->constIntDefined = true;
}

void sdfData::setConstantNumber(float constantNumber)
{
    this->constantNumber = constantNumber;
    this->constDefined = true;
}

void sdfData::setConstantString(string constantString)
{
    this->constantString = constantString;
    this->constDefined = true;
}

void sdfData::setDefaultBool(bool defaultBool)
{
    this->defaultBool = defaultBool;
    this->defaultDefined = true;
    this->defaultBoolDefined = true;
}

void sdfData::setDefaultInt(int64_t defaultInt)
{
    this->defaultInt = defaultInt;
    this->defaultDefined = true;
    this->defaultIntDefined = true;
}

void sdfData::setDefaultNumber(float defaultNumber)
{
    this->defaultNumber = defaultNumber;
    this->defaultDefined = true;
}

void sdfData::setDefaultString(string defaultString)
{
    this->defaultString = defaultString;
    this->defaultDefined = true;
}

vector<string> sdfData::getConstantStringArray() const
{
    return constantStringArray;
}

void sdfData::setConstantArray(vector<string> constantArray)
{
    this->constantStringArray = constantArray;
    this->constDefined = true;
}

vector<string> sdfData::getDefaultStringArray() const
{
    return defaultStringArray;
}

void sdfData::setDefaultArray(vector<string> defaultArray)
{
    this->defaultStringArray = defaultArray;
    this->defaultDefined = true;
}

void sdfData::setParentCommon(sdfCommon *parentCommon)
{
    this->parent = parentCommon;
}

sdfCommon* sdfData::getParentCommon() const
{
    return this->parent;
}

sdfInfoBlock* sdfInfoBlock::jsonToInfo(json input)
{
    for (json::iterator it = input.begin(); it != input.end(); ++it)
    {
        if (it.key() == "title" && !it.value().empty())
        {
            this->title = it.value();
        }
        else if (it.key() == "version" && !it.value().empty())
        {
            this->version = it.value();
        }
        else if (it.key() == "copyright" && !it.value().empty())
        {
            this->copyright = it.value();
        }
        else if (it.key() == "license" && !it.value().empty())
        {
            this->license = it.value();
        }
    }
    return this;
}

sdfNamespaceSection* sdfNamespaceSection::jsonToNamespace(json input)
{
    for (json::iterator it = input.begin(); it != input.end(); ++it)
    {
        if (it.key() == "namespace" && !it.value().empty())
        {
            for (json::iterator jt = it.value().begin(); jt != it.value().end(); ++jt)
            {
                if (!jt.value().empty())
                {
                    namespaces[jt.key()] = jt.value();
                    namedFiles[jt.key()] = prefixToFile[jt.key()];
                }
            }
        }
        else if (it.key() == "defaultNamespace" && !it.value().empty())
        {
            default_ns = it.value();
            namedFiles[default_ns] = NULL;
        }
    }
    return this;
}

const char* sdfCommon::getDescriptionAsArray()
{
    if (description != "")
        return description.c_str();
    return NULL;
}

std::map<const char*, const char*> sdfNamespaceSection::getNamespacesAsArrays()
{
    std::map<const char*, const char*> output;
    std::map<std::string, std::string>::iterator it = namespaces.begin();
    while (it != namespaces.end())
    {
        output[it->first.c_str()] = it->second.c_str();
        it++;
    }
    return output;
}

/*sdfNamespaceSection::~sdfNamespaceSection()
{
    //namespaces.clear();
    map<string, string>::iterator it;
    for (it = namespaces.begin(); it != namespaces.end(); it++)
    {
    }
    namespaces.clear();
}*/

const char* sdfNamespaceSection::getDefaultNamespaceAsArray()
{
    return this->default_ns.c_str();
}

const char* sdfInfoBlock::getTitleAsArray() const
{
    return this->title.c_str();
}

const char* sdfInfoBlock::getVersionAsArray() const
{
    return this->version.c_str();
}

const char* sdfInfoBlock::getCopyrightAsArray() const
{
    return this->copyright.c_str();
}

const char* sdfInfoBlock::getLicenseAsArray() const
{
    return this->license.c_str();
}

const char* sdfData::getUnitsAsArray() const
{
    return this->units.c_str();
}
/*
const char* sdfData::getDefaultAsCharArray()
{
    cout << "WRONG FUNCTION USED: getDefaultAsCharArray" << endl;
    if (!defaultDefined)
        return NULL;
    std::string *str;
    //if (simpleType == json_number)
    if (!isnan(defaultNumber))
    {
        str = new std::string(std::to_string(defaultNumber));
        defaultAsCharArray = str->c_str();
    }
    //else if (simpleType == json_string && defaultString != "")
    else if (defaultString != "")
        defaultAsCharArray = defaultString.c_str();

    //else if (simpleType == json_boolean)
    else if (defaultBoolDefined)
    {
        if (defaultBool == true)
            defaultAsCharArray = "true";
        else
            defaultAsCharArray = "false";
    }
    //else if (simpleType == json_integer)
    else if (defaultIntDefined)
    {
        str = new std::string(std::to_string(defaultInt));
        defaultAsCharArray = str->c_str();
    }
    //else if(simpleType == json_array)
    else if(!defaultBoolArray.empty() || !defaultIntArray.empty()
            || !defaultNumberArray.empty() || !defaultStringArray.empty())
    {
        // fill a vector with whatever default vector is defined
        // (only one of them can be != {})
        vector<string> strVec = defaultStringArray;
        for (float i : defaultNumberArray)
            strVec.push_back(to_string(i));
        for (bool i : defaultBoolArray)
        {
            if (i == true)
                strVec.push_back("true");
            else
                strVec.push_back("false");
        }
        for (int i : defaultIntArray)
            strVec.push_back(to_string(i));
        const char *result[strVec.size()];
        for (int i = 0; i < strVec.size(); i++)
            result[i] = strVec[i].c_str();
        defaultAsCharArray = (const char*)result;
    }

    return defaultAsCharArray;
}

const char* sdfData::getConstantAsCharArray()
{
    cout << "WRONG FUNCTION USED: getConstantAsCharArray" << endl;
    if (!constDefined)
        return NULL;
    std::string str;
    if (simpleType == json_number)
    {
        str = std::to_string(constantNumber);
        constantAsCharArray = str.c_str();
    }
    else if (simpleType == json_string && constantString != "")
        constantAsCharArray = constantString.c_str();

    else if (simpleType == json_boolean)
    {
        if (constantBool == true)
            constantAsCharArray = "true";
        else
            constantAsCharArray = "false";
    }
    else if (simpleType == json_integer)
    {
        str = std::to_string(constantInt);
        constantAsCharArray = str.c_str();
    }

    return constantAsCharArray;
}
*/
sdfData* sdfData::getItemConstr() const
{
    return item_constr;
}

sdfData* sdfData::getItemConstrOfRefs() const
{
    sdfData *ref = this->getSdfDataReference();
    if (ref && !item_constr)
        return ref->getItemConstrOfRefs();

    return item_constr;
}

void sdfData::addChoice(sdfData *choice)
{
    choice->setParentCommon(this);
    sdfChoice.push_back(choice);
}

void sdfData::addObjectProperty(sdfData *property)
{
    objectProperties.push_back(property);
    property->setParentCommon(this);
}

void sdfData::addRequiredObjectProperty(string propertyName)
{
    requiredObjectProperties.push_back(propertyName);
}

std::vector<sdfData*> sdfData::getChoice() const
{
    return sdfChoice;
}

void sdfData::setChoice(vector<sdfData*> choices)
{
    sdfChoice = choices;
    for (sdfData *choice : this->getChoice())
        choice->setParentCommon(this);
}

void sdfData::setConstantArray(std::vector<bool> constantArray)
{
    constantBoolArray = constantArray;
    constDefined = true;
}

void sdfData::setDefaultArray(std::vector<bool> defaultArray)
{
    defaultBoolArray = defaultArray;
    defaultDefined = true;
}

void sdfData::setConstantArray(std::vector<int64_t> constantArray)
{
    constantIntArray = constantArray;
    constDefined = true;
}

void sdfData::setDefaultArray(std::vector<int64_t> defaultArray)
{
    defaultIntArray = defaultArray;
    defaultDefined = true;
}

void sdfData::setConstantArray(std::vector<float> constantArray)
{
    constantNumberArray = constantArray;
    constDefined = true;
}

void sdfData::setDefaultArray(std::vector<float> defaultArray)
{
    defaultNumberArray = defaultArray;
    defaultDefined = true;
}

std::vector<bool> sdfData::getConstantBoolArray() const
{
    return constantBoolArray;
}

std::vector<bool> sdfData::getDefaultBoolArray() const
{
    return defaultBoolArray;
}

std::vector<int64_t> sdfData::getConstantIntArray() const
{
    return constantIntArray;
}

std::vector<int64_t> sdfData::getDefaultIntArray() const
{
    return defaultIntArray;
}

std::vector<float> sdfData::getDefaultNumberArray() const
{
    return defaultNumberArray;
}

std::vector<float> sdfData::getConstantNumberArray() const
{
    return constantNumberArray;
}

std::vector<sdfData*> sdfData::getObjectProperties() const
{
    return objectProperties;
}

std::vector<sdfData*> sdfData::getObjectPropertiesOfRefs() const
{
    sdfData *ref = this->getSdfDataReference();
    if (ref)
    {
        vector<sdfData*> refObjProps = ref->getObjectProperties();
        vector<sdfData*> concatObjProps = {};

        concatObjProps.insert(concatObjProps.end(),
                objectProperties.begin(),
                objectProperties.end());
        concatObjProps.insert(concatObjProps.end(),
                refObjProps.begin(),
                refObjProps.end());
        return concatObjProps;
    }
    return objectProperties;
}

void sdfData::setItemConstr(sdfData *constr)
{
    //constr->setLabel(this->getLabel() + "-items");
    // TODO: properties should also have this as their parent and not
    // the constraint
    if (constr)
    {
        item_constr = constr;
        item_constr->setParentCommon((sdfCommon*)this); // Problem
    }
}

void sdfData::setConstantObject(sdfData *object)
{
    constantObject = object;
    constDefined = true;
}

void sdfData::setDefaultObject(sdfData *object)
{
    defaultObject = object;
    defaultDefined = true;
}

sdfData* sdfData::getConstantObject() const
{
    return constantObject;
}

sdfData* sdfData::getDefaultObject() const
{
    return defaultObject;
}

void sdfData::setObjectProperties(std::vector<sdfData*> properties)
{
    objectProperties = properties;
    for (int i = 0; i < objectProperties.size(); i++)
        objectProperties.at(i)->setParentCommon(this);
}

std::string sdfCommon::getName() const
{
    if (name == "")
        return label;
    return name;
}

const char* sdfCommon::getNameAsArray() const
{
    if (name != "")
        return name.c_str();
    else
        return label.c_str();
    return NULL;
}

sdfFile* sdfCommon::getParentFile() const
{
    return parentFile;
}

void sdfCommon::setName(std::string _name)
{
    name = _name;
}

std::vector<std::string> sdfData::getRequiredObjectProperties() const
{
    return requiredObjectProperties;
}

void sdfData::setMinimum(double min)
{
    minimum = min;
}

void sdfData::setMaximum(double max)
{
    maximum = max;
}

void sdfData::setMultipleOf(float mult)
{
    multipleOf = mult;
}

bool sdfData::getReadableDefined() const
{
    return readableDefined;
}

bool sdfData::getWritableDefined() const
{
    return writableDefined;
}

bool sdfData::getNullableDefined() const
{
    return nullableDefined;
}

bool sdfData::getObservableDefined() const
{
    return observableDefined;
}

bool sdfData::getDefaultDefined() const
{
    return defaultDefined;
}

bool sdfData::getConstantDefined() const
{
    return constDefined;
}

bool sdfData::getDefaultIntDefined() const
{
    return defaultIntDefined;
}

bool sdfData::getConstantIntDefined() const
{
    return constIntDefined;
}

bool sdfData::getDefaultBoolDefined() const
{
    return defaultBoolDefined;
}

bool sdfData::getConstantBoolDefined() const
{
    return constBoolDefined;
}

bool sdfData::getUniqueItemsDefined() const
{
    return uniqueItemsDefined;
}

void sdfData::setMinLength(float min)
{
    minLength = min;
}

void sdfData::setMaxLength(float max)
{
    maxLength = max;
}

void sdfData::setEnumString(std::vector<std::string> enm)
{
    enumString = enm;
}

bool validateJson(json sdf, std::string schemaFileName)
{
    // Load the schema
    json sdf_schema;
    ifstream input_schema(schemaFileName);
    if (input_schema)
    {
        input_schema >>  sdf_schema;
        input_schema.close();
    }
    else
    {
        cerr << "Error opening validation CDDL file" << endl;
        return -1;
    }

    nlohmann::json_schema::json_validator validator;
    try
    {
        validator.set_root_schema(sdf_schema);
    }
    catch (const exception &e)
    {
        cerr << "Validation of schema failed:\n" << e.what() << endl;
        return -1;
    }

    // validate
    try
    {
        validator.validate(sdf);
    }
    catch (const exception &e)
    {
        cerr << "Validation failed:\n" << e.what() << endl;
        return false;
    }

    cout << "Validation succeeded" << endl;
    return true;
}

bool validateFile(std::string fileName, std::string schemaFileName)
{
    // open resulting model from file
    json sdf;
    ifstream input_sdf(fileName);
    if (input_sdf)
    {
        input_sdf >> sdf;
        input_sdf.close();
    }
    else
    {
        cerr << "Error opening sdf.json file" << endl;
        return -1;
    }
    cout << fileName << ": ";
    return validateJson(sdf, schemaFileName);
}

bool sdfData::isItemConstr() const
{
    sdfData* parent = dynamic_cast<sdfData*>(this->getParentCommon());
    if (parent && this == parent->getItemConstr())
        return true;

    return false;
}


bool sdfData::isObjectProp() const
{
    sdfData* parent = dynamic_cast<sdfData*>(this->getParentCommon());
    //if (parent && this == parent->getItemConstr())
    if (parent)
    {
        vector<sdfData*> op = parent->getObjectProperties();
        if (find(op.begin(), op.end(), this) != op.end())
            return true;
    }
    return false;
}

void sdfData::parseDefaultArray(lys_node_leaflist *node)
{
    if (!node)
    {
        cerr << "parseDefaultArray: node must not be null" << endl;
        return;
    }
    else if (this->getSimpType() != json_array)
    {
        cerr << "parseDefaultArray called for type != array" << endl;
        return;
    }
    else if(this->getSimpType() == json_array && node->dflt_size > 0)
    {
        defaultDefined = true;
        //cout<<"dflt size "<<to_string(node->dflt_size)<<endl;

        if (this->getItemConstr()->getSimpType() == json_string)
        {
            for (int i = 0; i < node->dflt_size; i++)
            {
                defaultStringArray.push_back(node->dflt[i]);
                //cout <<"dflt "<< node->dflt[i] << endl;
            }
        }

        else if (this->getItemConstr()->getSimpType() == json_number)
        {
            for (int i = 0; i < node->dflt_size; i++)
                defaultNumberArray.push_back(atof(node->dflt[i]));
        }
        else if (this->getItemConstr()->getSimpType() == json_boolean)
        {
            defaultBoolArray = vector<bool>(node->dflt_size, false);
            for (int i = 0; i < node->dflt_size; i++)
                if (strcmp(node->dflt[i], "true") == 0)
                    defaultBoolArray[i] = true;
        }
        else if (this->getItemConstr()->getSimpType() == json_integer)
        {
            for (int i = 0; i < node->dflt_size; i++)
                defaultIntArray.push_back(atoi(node->dflt[i]));
        }

        else if (this->getItemConstr()->getSimpType() == json_array)
            cerr
            << "parseDefault: array should not have objects of type array,"
                    " default cannot be parsed"
            << endl;

        else if (this->getItemConstr()->getSimpType() == json_object)
            cerr << "parseDefault: called by default of object but"
                    "should only be used by leafs" << endl;
    }
}
/*
 * Translate the default value given by a leaf node as char array
 * into the type that corresponds to the type of this sdfData element.t
 */
void sdfData::parseDefault(const char *value)
{
    if (!value)
        return;

    if (this->getSimpType() == json_string)
        this->setDefaultString(value);

    else if (value != "")
    {
        if(this->getSimpType() == json_number)
            this->setDefaultNumber(atof(value));

        else if(this->getSimpType() == json_boolean)
        {
            if (strcmp(value, "true") == 0)
                this->setDefaultBool(true);
            else if (strcmp(value, "false") == 0)
                this->setDefaultBool(false);
        }

        else if(this->getSimpType() == json_integer)
            this->setDefaultInt(atoi(value));
    }

    if (this->getSimpType() == json_object)
        cerr << "parseDefault: called by default of object but"
                "should only be used by leafs" << endl;
}

string sdfData::getDefaultAsString()
{
    if (!defaultDefined)
        return "";

    string defStr;
    if (simpleType == json_number || !isnan(defaultNumber))
    {
        defStr = to_string(defaultNumber);
    }
    else if (simpleType == json_string || defaultString != "")
    {
        defStr = defaultString;
    }

    else if (simpleType == json_boolean || defaultBoolDefined)
    {
        if (defaultBool == true)
            defStr = "true";
        else
            defStr = "false";
    }
    else if (simpleType == json_integer || defaultIntDefined)
    {
        defStr = string(to_string(defaultInt));
    }
    else if(simpleType == json_array || !defaultBoolArray.empty()
            || !defaultIntArray.empty() || !defaultNumberArray.empty()
            || !defaultStringArray.empty())
    {
        cerr << "getDefaultAsString: wrong function for default arrays" << endl;
    }

    return defStr;
}

vector<string> sdfData::getDefaultArrayAsStringVector()
{
    // fill a vector with whatever default vector is defined
    // (only one of them can be != {})
    vector<string> strVec = defaultStringArray;

    for (float i : defaultNumberArray)
        strVec.push_back(to_string(i));

    for (bool i : defaultBoolArray)
    {
        if (i == true)
            strVec.push_back("true");
        else
            strVec.push_back("false");
    }

    for (int i : defaultIntArray)
        strVec.push_back(to_string(i));

    return strVec;
}

string sdfData::getConstantAsString()
{
    if (!constDefined)
        return "";

    string constStr;
    if (simpleType == json_number || !isnan(constantNumber))
    {
        constStr = to_string(constantNumber);
    }
    else if (simpleType == json_string || constantString != "")
        constStr = constantString;

    else if (simpleType == json_boolean || constBoolDefined)
    {
        if (constantBool == true)
            constStr = "true";
        else
            constStr = "false";
    }
    else if (simpleType == json_integer || constIntDefined)
    {
        constStr = to_string(constantInt);
    }
    else if(simpleType == json_array || !constantBoolArray.empty()
            || !constantIntArray.empty() || !constantNumberArray.empty()
            || !constantStringArray.empty())
    {
        cerr << "getDefaultAsString: wrong function for constant arrays"
                << endl;
    }

    return constStr;
}

void sdfCommon::setParentFile(sdfFile *file)
{
    parentFile = file;
}

sdfFile::sdfFile()
{
    info = NULL;
    ns = NULL;
    things = {};
    objects = {};
    properties = {};
    actions = {};
    events = {};
    datatypes = {};
}

sdfFile::~sdfFile()
{
    delete info;
    info = NULL;
    delete ns;
    ns = NULL;

    for (int i = 0; i < things.size(); i++)
    {
        delete things[i];
        things[i] = NULL;
    }
    things.clear();

    for (int i = 0; i < objects.size(); i++)
    {
        delete objects[i];
        objects[i] = NULL;
    }
    objects.clear();

    for (int i = 0; i < properties.size(); i++)
    {
        delete properties[i];
        properties[i] = NULL;
    }
    properties.clear();

    for (int i = 0; i < actions.size(); i++)
    {
        delete actions[i];
        actions[i] = NULL;
    }
    actions.clear();

    for (int i = 0; i < events.size(); i++)
    {
        delete events[i];
        events[i] = NULL;
    }
    events.clear();

    for (int i = 0; i < datatypes.size(); i++)
    {
        delete datatypes[i];
        datatypes[i] = NULL;
    }
    datatypes.clear();
}

void sdfFile::setInfo(sdfInfoBlock *_info)
{
    info = _info;
}

void sdfFile::setNamespace(sdfNamespaceSection *_ns)
{
    ns = _ns;
}

void sdfFile::addThing(sdfThing *thing)
{
    thing->setParentThing(NULL);
    thing->setParentFile(this);
    things.push_back(thing);
}

void sdfFile::addObject(sdfObject *object)
{
    object->setParentThing(NULL);
    object->setParentFile(this);
    objects.push_back(object);
}

void sdfFile::addProperty(sdfProperty *property)
{
    property->setParentCommon(NULL);
    property->setParentObject(NULL);
    property->setParentFile(this);
    properties.push_back(property);
}

void sdfFile::addAction(sdfAction *action)
{
    action->setParentObject(NULL);
    action->setParentFile(this);
    actions.push_back(action);
}

void sdfFile::addEvent(sdfEvent *event)
{
    event->setParentObject(NULL);
    event->setParentFile(this);
    events.push_back(event);
}

void sdfFile::addDatatype(sdfData *datatype)
{
    datatype->setParentCommon(NULL);
    datatype->setParentFile(this);
    datatypes.push_back(datatype);
}

sdfInfoBlock* sdfFile::getInfo() const
{
    return info;
}

sdfNamespaceSection* sdfFile::getNamespace() const
{
    return ns;
}

std::vector<sdfThing*> sdfFile::getThings() const
{
    return things;
}

std::vector<sdfObject*> sdfFile::getObjects() const
{
    return objects;
}

std::vector<sdfProperty*> sdfFile::getProperties()
{
    return properties;
}

std::vector<sdfAction*> sdfFile::getActions()
{
    return actions;
}

std::vector<sdfEvent*> sdfFile::getEvents()
{
    return events;
}

std::vector<sdfData*> sdfFile::getDatatypes()
{
    return datatypes;
}

std::string sdfFile::generateReferenceString(sdfCommon *child, bool import)
{
    string childRef = "";
    if (child)
    {
        if (find(things.begin(), things.end(), child) != things.end())
            childRef = "/sdfThing/" + child->getName();

        else if (find(objects.begin(), objects.end(), child) != objects.end())
            childRef = "/sdfObject/" + child->getName();

        else if (find(datatypes.begin(), datatypes.end(), child) != datatypes.end())
            childRef = "/sdfData/" + child->getName();

        else if (find(properties.begin(), properties.end(), child)
                != properties.end())
            childRef = "/sdfProperty/" + child->getName();

        else if (find(actions.begin(), actions.end(), child) != actions.end())
            childRef = "/sdfAction/" + child->getName();

        else if (find(events.begin(), events.end(), child) != events.end())
            childRef = "/sdfEvent/" + child->getName();

        else
            cerr << "sdfFile::generateReferenceString: " + child->getName()
            + " references file but could not be found in file" << endl;
    }
    if (import && this->getNamespace()
            && this->getNamespace()->getDefaultNamespace() != "")
        return this->getNamespace()->getDefaultNamespace() + ":" + childRef;

    return "#" + childRef;
}

nlohmann::json sdfFile::toJson(nlohmann::json prefix)
{
    // print info if specified by print_info
    if (this->info)
        prefix = this->info->infoToJson(prefix);
    if (this->ns)
        prefix = this->ns->namespaceToJson(prefix);

    for (sdfThing *i : things)
        prefix["sdfThing"][i->getName()] =
                i->thingToJson({}, false)["sdfThing"][i->getName()];

    for (sdfObject *i : objects)
        prefix["sdfObject"][i->getName()] =
                i->objectToJson({}, false)["sdfObject"][i->getName()];

    for (sdfData *i : datatypes)
        prefix["sdfData"][i->getName()] =
                i->dataToJson({})["sdfData"][i->getName()];

    for (sdfProperty *i : properties)
        prefix["sdfProperty"][i->getName()] =
                i->propertyToJson({})["sdfProperty"][i->getName()];

    for (sdfAction *i : actions)
        prefix["sdfAction"][i->getName()] =
                i->actionToJson({})["sdfAction"][i->getName()];

    for (sdfEvent *i : events)
        prefix["sdfEvent"][i->getName()] =
                i->eventToJson({})["sdfEvent"][i->getName()];

    return prefix;
}

std::string sdfFile::toString()
{
    json json_output;
    return this->toJson(json_output).dump(INDENT_WIDTH);
}

void sdfFile::toFile(std::string path)
{
    ofstream output(path);
    if (output)
    {
        output << this->toString() << endl;
        output.close();
    }
    else
        cerr << "Error opening file" << endl;

    validateFile(path);
}

sdfFile* sdfFile::fromJson(nlohmann::json input)
{
    // first check for the namespace etc (to determine whether this file
    // contributes to a global namespace -> whether default namespace is given)
    for (json::iterator it = input.begin(); it != input.end(); ++it)
    {
        if (it.key() == "info" && !it.value().empty())
        {
            //shared_ptr<sdfInfoBlock> info(new sdfInfoBlock());
            sdfInfoBlock *info = new sdfInfoBlock();
            this->setInfo(info);
            info->jsonToInfo(input["info"]);
        }
        else if (it.key() == "namespace" && !it.value().empty())
        {
            //shared_ptr<sdfNamespaceSection> ns(new sdfNamespaceSection());
            sdfNamespaceSection *ns = new sdfNamespaceSection();
            this->setNamespace(ns);
            ns->jsonToNamespace(input);
        }
        else if (it.key() == "defaultNamespace" && !it.value().empty())
        {
            ns->jsonToNamespace(input["namespace"]);
        }
    }
    if (isContext && (!ns || ns->getDefaultNamespace() == ""))
        return NULL;

    // then check for things etc
    for (json::iterator it = input.begin(); it != input.end(); ++it)
    {
        if (it.key() == "sdfThing" && !it.value().empty())
        {
            for (json::iterator jt = it.value().begin();
                    jt != it.value().end(); ++jt)
            {
                sdfThing *childThing = new sdfThing();
                childThing->setName(correctValue(jt.key()));
                this->addThing(childThing);
                childThing->jsonToThing(input["sdfThing"][jt.key()], true);
            }
        }
        else if (it.key() == "sdfObject" && !it.value().empty())
        {
            for (json::iterator jt = it.value().begin();
                    jt != it.value().end(); ++jt)
            {
                sdfObject *childObject = new sdfObject();
                childObject->setName(correctValue(jt.key()));
                this->addObject(childObject);
                childObject->jsonToObject(input["sdfObject"][jt.key()], true);
            }
        }
        else if (it.key() == "sdfProperty" && !it.value().empty())
        {
            for (json::iterator jt = it.value().begin();
                    jt != it.value().end(); ++jt)
            {
                sdfProperty *childProperty = new sdfProperty();
                this->addProperty(childProperty);
                childProperty->setName(correctValue(jt.key()));
                childProperty->jsonToProperty(input["sdfProperty"][jt.key()]);
            }
        }
        else if (it.key() == "sdfAction" && !it.value().empty())
        {
            for (json::iterator jt = it.value().begin();
                    jt != it.value().end(); ++jt)
            {
                sdfAction *childAction = new sdfAction();
                this->addAction(childAction);
                childAction->setName(correctValue(jt.key()));
                childAction->jsonToAction(input["sdfAction"][jt.key()]);
            }
        }
        else if (it.key() == "sdfEvent" && !it.value().empty())
        {
            for (json::iterator jt = it.value().begin();
                    jt != it.value().end(); ++jt)
            {
                sdfEvent *childEvent =  new sdfEvent();
                this->addEvent(childEvent);
                // TODO: with label set this way, a label will be printed
                // even though there was just a "title" in the original
                childEvent->setName(correctValue(jt.key()));
                childEvent->jsonToEvent(input["sdfEvent"][jt.key()]);
            }
        }
        else if (it.key() == "sdfData" && !it.value().empty())
        {
            for (json::iterator jt = it.value().begin();
                    jt != it.value().end(); ++jt)
            {
                sdfData *childData = new sdfData();
                this->addDatatype(childData);
                childData->setName(correctValue(jt.key()));
                childData->jsonToData(input["sdfData"][jt.key()]);
            }
        }
    }
    // assign sdfRef and sdfRequired references
    unassignedRefs = assignRefs(unassignedRefs, REF);
    unassignedReqs = assignRefs(unassignedReqs, REQ);

    if (!unassignedRefs.empty() || !unassignedReqs.empty())
        cerr << "There is/are "
        + to_string(unassignedRefs.size()+unassignedReqs.size())
        + " reference(s) left unassigned" << endl;
    else
        cout << "All references resolved" << endl;

    if (isContext)
    {
//        unassignedRefs = {};
//        unassignedReqs = {};

        if (this->getNamespace())
            this->getNamespace()->makeDefinitionsGlobal();
    }

    return this;
}

sdfFile* sdfFile::fromFile(std::string path)
{
    if (!contextLoaded)
        loadContext();

    json json_input;
    ifstream input(path);
    if (input)
    {
        input >>  json_input;
        input.close();
    }
    else
    {
        cerr << "Error opening file" << endl;
        return NULL;
    }
    return this->fromJson(json_input);
}

sdfFile* sdfCommon::getTopLevelFile()
{
    sdfCommon *parent = this;
    while (parent->getParent() && !parent->getParentFile())
        parent = parent->getParent();

    return parent->getParentFile();
}

sdfCommon* sdfObjectElement::getParent() const
{
    return (sdfCommon*)this->getParentObject();
}

sdfCommon* sdfData::getParent() const
{

    return this->getParentCommon();
}

sdfCommon* sdfObject::getParent() const
{
    return (sdfCommon*)this->getParentThing();
}

sdfCommon* sdfThing::getParent() const
{
    return (sdfCommon*)this->getParentThing();
}

sdfCommon* sdfProperty::getParent() const
{
    return this->sdfObjectElement::getParent();
}

std::string sdfCommon::generateReferenceString(bool import)
{
    if (this->getParent())
    {
        //cout << this->getParent()->generateReferenceString(this) << endl;
        return this->getParent()->generateReferenceString(this, import);
    }
    else if (this->getParentFile())
    {
        //cout << this->getParentFile()->generateReferenceString(this) << endl;
        return this->getParentFile()->generateReferenceString(this, import);
    }

    cerr << "sdfCommon::generateReferenceString: " + this->getName()
            + " has no assigned parent object" << endl;
    return "";
}

sdfData* sdfCommon::getThisAsSdfData()
{
    return NULL;
}

sdfData* sdfData::getThisAsSdfData()
{
    return this;
}

void sdfNamespaceSection::addNamespace(std::string pre, std::string ns)
{
    namespaces[pre] = ns;

    // link files no foreign namespace
    namedFiles[pre] = prefixToFile[pre];

    if (pre == default_ns)
        namedFiles[pre] = NULL;
}

void sdfData::setUniqueItems(bool unique)
{
    uniqueItems = unique;
    uniqueItemsDefined = true;
}

std::map<std::string, sdfFile*> sdfNamespaceSection::getNamedFiles() const
{
    return namedFiles;
}

string sdfCommon::getDefaultNamespace()
{
    sdfFile *top = this->getTopLevelFile();
    if (top)
        return top->getNamespace()->getDefaultNamespace();

    return "";
}

map<string, string> sdfCommon::getNamespaces()
{
    sdfFile *top = this->getTopLevelFile();
    if (top)
        return top->getNamespace()->getNamespaces();

    return {};
}

void sdfNamespaceSection::updateNamedFiles()
{
    // link files no foreign namespaces
    map<string, string>::iterator it;
    for (it = namespaces.begin(); it != namespaces.end(); it++)
        namedFiles[it->first] = prefixToFile[it->first];

    namedFiles[default_ns] = NULL;
}

void sdfData::setWritable(bool _writable)
{
    writable = _writable;
    writableDefined = true;
}

void sdfData::setReadable(bool _readable)
{
    readable = _readable;
    readableDefined = true;
}

void sdfNamespaceSection::removeNamespace(string pre)
{
    namespaces.erase(namespaces.find(pre));
    namedFiles.erase(namedFiles.find(pre));
}

void sdfData::setMinInt(int64_t min)
{
    minInt = min;
    minIntSet = true;
}

void sdfData::setMaxInt(uint64_t max)
{
    maxInt = max;
    maxIntSet = true;
}

int64_t sdfData::getMinInt() const
{
    return minInt;
}

uint64_t sdfData::getMaxInt() const
{
    return maxInt;
}

void sdfData::eraseMinInt()
{
    minInt = 0;
    minIntSet = false;
}

void sdfData::eraseMaxInt()
{
    maxInt = 0;
    maxIntSet = false;
}

bool sdfData::getMinIntSet() const
{
    return minIntSet;
}

bool sdfData::getMaxIntSet() const
{
    return maxIntSet;
}
