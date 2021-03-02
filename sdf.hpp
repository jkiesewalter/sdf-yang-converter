#ifndef SDF_H
#define SDF_H

#include <iostream>
#include <fstream>
#include<sstream>
#include<stdio.h>
#include<string>
#include<vector>
#include <cmath>
#include <regex>
#include <typeinfo>
#include <nlohmann/json.hpp>

enum jsonDataType
{
    json_number,
    json_string,
    json_boolean,
    json_integer,
    json_array,
    json_object,
    json_type_undef
};
enum jsonSchemaFormat
{
    json_date_time,
    json_date,
    json_time,
    json_uri,
    json_uri_reference,
    json_uuid,
    json_format_undef
};
enum sdfSubtype
{
    sdf_byte_string,
    sdf_unix_time,
    sdf_subtype_undef
};

class sdfCommon;
class sdfThing;
class sdfObject;
class sdfProperty;

std::string jsonDTypeToString(jsonDataType type);
jsonDataType stringToJsonDType(std::string str);
sdfCommon* refToCommon(std::string ref);
std::string  correctValue(std::string val);

#define INDENT_WIDTH 2

// sdfObject, sdfProperty, sdfAction, sdfEvent and sdfData are sdfCommons
class sdfCommon
{
public:
    sdfCommon(std::string _label = "", std::string _description = "",
            sdfCommon *_reference = NULL, std::vector<sdfCommon*> _required = {});
    virtual ~sdfCommon();
    // getters
    std::string getDescription();
    const char* getDescriptionAsArray();
    std::string getLabel();
    const char* getLabelAsArray();
    sdfCommon* getReference();
    std::vector<sdfCommon*> getRequired();
    //sdfCommon* getParentCommon();
    // setters
    void setLabel(std::string _label);
    void setDescription(std::string dsc);
    void addRequired(sdfCommon *common);
    void setReference(sdfCommon *common);
    //void setParentCommon(sdfCommon *parentCommon);
    // printing
    virtual std::string generateReferenceString() = 0;
    nlohmann::json commonToJson(nlohmann::json prefix);
    void jsonToCommon(nlohmann::json input);
private:
    std::string description;
    std::string label;
    sdfCommon *reference;
    std::vector<sdfCommon*> required;
    //sdfCommon *parent;
};

class sdfObjectElement : virtual public sdfCommon
{
public:
    sdfObjectElement(std::string _label = "", std::string _description = "",
            sdfCommon *_reference = NULL, std::vector<sdfCommon*> _required = {},
            sdfObject *_parentObject = NULL);
    ~sdfObjectElement();
    sdfObject* getParentObject();
    void setParentObject(sdfObject *parentObject);
    std::string generateReferenceString();
private:
    sdfObject *parentObject;
};

class sdfInfoBlock
{
public:
    sdfInfoBlock(std::string _title = "", std::string _version = "",
                std::string _copyright = "", std::string _license= "");
    // getters
    std::string getTitle();
    std::string getVersion();
    std::string getCopyright();
    std::string getLicense();
    const char* getTitleAsArray() const;
    const char* getVersionAsArray() const;
    const char* getCopyrightAsArray() const;
    const char* getLicenseAsArray() const;
    // parsing
    nlohmann::json infoToJson(nlohmann::json prefix);
    sdfInfoBlock* jsonToInfo(nlohmann::json input);
private:
    std::string title;
    std::string version;
    std::string copyright;
    std::string license;
};

class sdfNamespaceSection
{
public:
    sdfNamespaceSection(std::map<std::string, std::string> _namespaces = {},
                        std::string _default_ns = "");
    // getters
    std::map<std::string, std::string> getNamespaces();
    std::map<const char*, const char*> getNamespacesAsArrays();
    std::string getDefaultNamespace();
    const char* getDefaultNamespaceAsArray();
    // parsing
    nlohmann::json namespaceToJson(nlohmann::json prefix);
    sdfNamespaceSection* jsonToNamespace(nlohmann::json input);
private:
    std::map<std::string, std::string> namespaces;
    std::string default_ns;
};

class sdfData : virtual public sdfCommon
{
public:
    sdfData();
    sdfData(std::string _label,
            std::string _description,
            std::string _type,
            sdfCommon *_reference = NULL,
            std::vector<sdfCommon*> _required = {},
            sdfCommon *_parentCommon = NULL,
            std::vector<sdfData*> _choice = {});
    sdfData(std::string _label,
            std::string _description,
            jsonDataType _type,
            sdfCommon *_reference = NULL,
            std::vector<sdfCommon*> _required = {},
            sdfCommon *_parentCommon = NULL,
            std::vector<sdfData*> _choice = {});
    ~sdfData();
    // Setters for member variables (different for types)
    // check which of the sdfData data qualities are designated for which
    // data type (number, string, ...)
    void setNumberData(float _constant = NAN,
            float _default = NAN,
            float _min = NAN,
            float _max = NAN,
            float _multipleOf = NAN);
    // TODO: constant and default should not be set at the same time?
    void setStringData(std::string _constant = "",
                       std::string _default = "",
                       float _minLength = NAN,
                       float _maxLength = NAN,
                       std::string _pattern = "",
                       jsonSchemaFormat _format = json_format_undef,
                       std::vector<std::string> _enum = {});
    void setBoolData(bool _constant = false,
                     bool defineConst = false,
                     bool _default = false,
                     bool defineDefault = false);
    void setIntData(int _constant = -1,
                    bool defineConst = false,
                    int _default = -1,
                    bool defineDefault = false,
                    float _min = NAN,
                    float _max = NAN);
    void setArrayData(float _minItems = NAN,
                      float _maxItems = NAN,
                      bool _uniqueItems = NAN,
                      sdfData *_itemConstr = NULL);
    /*void setArrayData(std::vector<std::string> enm,
                      float _minItems = NAN,
                      float _maxItems = NAN,
                      bool _uniqueItems = NAN,
                         std::string item_type = "string",
                      sdfCommon *ref = NULL,
                      float minLength = NAN,
                      float maxLength = NAN,
                      jsonSchemaFormat format = json_format_undef);
    void setArrayData(std::vector<float> enm,
                      float _minItems = NAN,
                      float _maxItems = NAN,
                      bool _uniqueItems = NAN,
                      std::string item_type = "number",
                      sdfCommon *ref = NULL,
                      float min = NAN,
                      float max = NAN);
    void setArrayData(std::vector<int> enm,
                      float _minItems = NAN,
                      float _maxItems = NAN,
                      bool _uniqueItems = NAN,
                      std::string item_type = "integer",
                      sdfCommon *ref = NULL,
                      float min = NAN,
                      float max = NAN);*/
    // other setters
    void setSimpType(jsonDataType _type);
    void setType(std::string _type);
    void setType(jsonDataType _type);
    void setDerType(std::string _type);
    void setUnits(std::string _units, float _scaleMin = NAN, float _scaleMax = NAN);
    void setReadWrite(bool _readable, bool _writable);
    void setObserveNull(bool _observable, bool _nullable);
    void setFormat(jsonSchemaFormat _format);
    void setSubtype(sdfSubtype _subtype);
    void setMaxItems(float maxItems);
    void setMinItems(float minItems);
    void setPattern(std::string pattern);
    void setConstantBool(bool constantBool);
    void setConstantInt(int constantInt);
    void setConstantNumber(float constantNumber);
    void setConstantString(std::string constantString);
    void setDefaultBool(bool defaultBool);
    void setDefaultInt(int defaultInt);
    void setDefaultNumber(float defaultNumber);
    void setDefaultString(std::string defaultString);
    void setConstantArray(std::vector<bool> constantArray);
    void setDefaultArray(std::vector<bool> defaultArray);
    void setConstantArray(std::vector<int> constantArray);
    void setDefaultArray(std::vector<int> defaultArray);
    void setConstantArray(std::vector<float> constantArray);
    void setDefaultArray(std::vector<float> defaultArray);
    void setConstantArray(std::vector<std::string> constantArray);
    void setDefaultArray(std::vector<std::string> defaultArray);
    void setConstantObject(sdfData *object);
    void setDefaultObject(sdfData *object);
    // Todo: constant and default needed for object-type? No?
    //void setConstantArray(std::vector<sdfData*> constantArray);
    //void setDefaultArray(std::vector<sdfData*> defaultArray);
    void setItemConstr(sdfData* constr);
    void setParentCommon(sdfCommon *parentCommon);
    void setChoice(std::vector<sdfData*> choices);
    void addChoice(sdfData *choice);
    void addObjectProperty(sdfData *property);
    void setObjectProperties(std::vector<sdfData*> properties);
    void addRequiredObjectProperty(std::string propertyName);
    // getters for member variables
    bool getReadable();
    bool getWritable();
    bool getNullable();
    bool getObservable();
    bool getConstantBool();
    int getConstantInt();
    float getConstantNumber();
    std::string getConstantString();
    sdfData* getConstantObject() const;
    const char * getConstantAsCharArray();
    std::string getContentFormat();
    bool getDefaultBool();
    int getDefaultInt();
    float getDefaultNumber();
    std::string getDefaultString();
    sdfData* getDefaultObject() const;
    const char * getDefaultAsCharArray();
    //std::vector<bool> getEnumBool();
    //std::vector<int> getEnumInt();
    //std::vector<float> getEnumNumber();
    std::vector<std::string> getEnumString();
    bool isExclusiveMaximumBool();
    float getExclusiveMaximumNumber();
    bool isExclusiveMinimumBool();
    float getExclusiveMinimumNumber();
    jsonSchemaFormat getFormat();
    float getMaximum();
    float getMaxItems();
    float getMaxLength();
    float getMinimum();
    float getMinItems();
    float getMinLength();
    float getMultipleOf();
    std::string getPattern();
    float getScaleMaximum();
    float getScaleMinimum();
    sdfSubtype getSubtype();
    jsonDataType getSimpType();
    std::string getType();
    bool getUniqueItems();
    std::string getUnits();
    const char* getUnitsAsArray() const;
    std::vector<bool> getConstantBoolArray() const;
    std::vector<bool> getDefaultBoolArray() const;
    std::vector<int> getConstantIntArray() const;
    std::vector<int> getDefaultIntArray() const;
    std::vector<float> getConstantNumberArray() const;
    std::vector<float> getDefaultNumberArray() const;
    std::vector<std::string> getConstantStringArray() const;
    std::vector<std::string> getDefaultStringArray() const;
    sdfCommon* getParentCommon();
    sdfData* getItemConstr() const;
    std::vector<sdfData*> getChoice() const;
    std::vector<sdfData*> getObjectProperties() const;
    // parsing
    std::string generateReferenceString();
    nlohmann::json dataToJson(nlohmann::json prefix);
    sdfData* jsonToData(nlohmann::json input);

private:
    bool constDefined;
    bool defaultDefined;
    jsonDataType simpleType;
    //std::vector<jsonDataType> unionTypes;
    std::string derType;
    // only fill for type number
    //std::vector<float> enumNumber;
    float constantNumber;
    float defaultNumber;
    // only fill for type string
    std::vector<std::string> enumString;
    std::string constantString;
    std::string defaultString;
    // only fill for type boolean
    //std::vector<bool> enumBool; // TODO: does this even make sense?
    bool constantBool;
    bool defaultBool;
    bool constBoolDefined;
    bool defaultBoolDefined;
    // only fill for type integer
    //std::vector<int> enumInt;
    int constantInt;
    int defaultInt;
    bool constIntDefined;
    bool defaultIntDefined;
    // only fill for type array
    // TODO: find a way to represent not only string arrays as const/default
    std::vector<bool> constantBoolArray;
    std::vector<bool> defaultBoolArray;
    std::vector<int> constantIntArray;
    std::vector<int> defaultIntArray;
    std::vector<float> constantNumberArray;
    std::vector<float> defaultNumberArray;
    std::vector<std::string> constantStringArray;
    std::vector<std::string> defaultStringArray;
    sdfData *constantObject;
    sdfData *defaultObject;
    /*std::vector<std::vector<auto>> enumArray;
    std::vector<auto> constantArray;
    std::vector<auto> defaultArray;*/
    const char *constantAsCharArray;
    const char *defaultAsCharArray;
    float minimum;
    float maximum;
    float exclusiveMinimum_number; // TODO: I don't get this
    float exclusiveMaximum_number;
    bool exclusiveMinimum_bool;
    bool exclusiveMaximum_bool;
    float multipleOf;
    float minLength;
    float maxLength;
    std::string pattern; // regex?
    jsonSchemaFormat format;
    float minItems;
    float maxItems;
    bool uniqueItems;
    bool uniqueItemsDefined;
    sdfData *item_constr;
    // SDF-defined data qualities
    std::string units;
    float scaleMinimum;
    float scaleMaximum;
    bool readable;
    bool writable;
    bool observable;
    bool nullable;
    bool readableDefined;
    bool writableDefined;
    bool observableDefined;
    bool nullableDefined;
    std::string contentFormat;
    // TODO: rename sdfSubtype to sdfType?
    sdfSubtype subtype;
    sdfCommon *parent;
    std::vector<sdfData*> objectProperties;
    std::vector<std::string> requiredObjectProperties;
    std::vector<sdfData*> sdfChoice;
};

class sdfEvent : virtual public sdfObjectElement
{
public:
    sdfEvent(std::string _label = "",
            std::string _description = "",
            sdfCommon *_reference = NULL,
            std::vector<sdfCommon*> _required = {},
            sdfObject *_parentObject = NULL,
            sdfData* _outputData = NULL,
            std::vector<sdfData*> _datatypes = {});
    virtual ~sdfEvent();
    // setters
    void setOutputData(sdfData *outputData);
    void addDatatype(sdfData *datatype);
    // getters
    std::vector<sdfData*> getDatatypes();
    sdfData* getOutputData();
    // parsing
    std::string generateReferenceString();
    nlohmann::json eventToJson(nlohmann::json prefix);
    sdfEvent* jsonToEvent(nlohmann::json input);
private:
    sdfData *outputData;
    std::vector<sdfData*> datatypes;
};

class sdfAction : virtual public sdfObjectElement
{
public:
    sdfAction(std::string _label = "",
            std::string _description = "",
            sdfCommon *_reference = NULL,
            std::vector<sdfCommon*> _required = {},
            sdfObject *_parentObject = NULL,
            sdfData* _inputData = NULL,
            std::vector<sdfData*> _requiredInputData = {},
            sdfData* _outputData = NULL,
            std::vector<sdfData*> _datatypes = {});
    virtual ~sdfAction();
    // setters
    void setInputData(sdfData* inputData);
    void addRequiredInputData(sdfData* requiredInputData);
    void setOutputData(sdfData* outputData);
    void addDatatype(sdfData *datatype);
    // getters
    sdfData* getInputData();
    std::vector<sdfData*> getRequiredInputData();
    sdfData* getOutputData();
    std::vector<sdfData*> getDatatypes();
    // parsing
    std::string generateReferenceString();
    nlohmann::json actionToJson(nlohmann::json prefix);
    sdfAction* jsonToAction(nlohmann::json input);

private:
    sdfData *inputData;
    std::vector<sdfData*> requiredInputData; // TODO: obsolete?
    sdfData *outputData;
    std::vector<sdfData*> datatypes;
};

class sdfProperty : public sdfData, virtual public sdfObjectElement
{
public:
    sdfProperty(std::string _label = "",
            std::string _description = "",
            jsonDataType _type = {},
            sdfCommon *_reference = NULL,
            std::vector<sdfCommon*> _required = {},
             sdfObject *_parentObject = NULL);
    sdfProperty(sdfData& data);
    // parsing
    std::string generateReferenceString();
    nlohmann::json propertyToJson(nlohmann::json prefix);
    sdfProperty* jsonToProperty(nlohmann::json input);
};

class sdfObject : virtual public sdfCommon
{
public:
    sdfObject(std::string _label = "", std::string _description = "",
            sdfCommon *_reference = NULL, std::vector<sdfCommon*> _required = {},
            std::vector<sdfProperty*> _properties = {},
            std::vector<sdfAction*> _actions = {}, std::vector<sdfEvent*> _events = {},
            std::vector<sdfData*> _datatypes = {}, sdfThing *_parentThing = NULL);
    virtual ~sdfObject();
    // setters
    void setInfo(sdfInfoBlock *_info);
    void setNamespace(sdfNamespaceSection *_ns);
    void addProperty(sdfProperty *property);
    void addAction(sdfAction *action);
    void addEvent(sdfEvent *event);
    void addDatatype(sdfData *datatype);
    void setParentThing(sdfThing *parentThing);
    // getters
    sdfInfoBlock* getInfo();
    sdfNamespaceSection* getNamespace();
    std::vector<sdfProperty*> getProperties();
    std::vector<sdfAction*> getActions();
    std::vector<sdfEvent*> getEvents();
    std::vector<sdfData*> getDatatypes();
    sdfThing* getParentThing();
    // parsing
    std::string generateReferenceString();
    nlohmann::json objectToJson(nlohmann::json prefix, bool print_info_namespace = true);
    std::string objectToString(bool print_info_namespace = true);
    void objectToFile(std::string path);
    sdfObject* jsonToObject(nlohmann::json input);
    sdfObject* fileToObject(std::string path);

private:
    nlohmann::json actionToJson();
    nlohmann::json eventToJson();
    sdfInfoBlock *info;
    sdfNamespaceSection *ns;
    std::vector<sdfProperty*> properties;
    std::vector<sdfAction*> actions;
    std::vector<sdfEvent*> events;
    std::vector<sdfData*> datatypes;
    sdfThing *parent;
};

class sdfThing : virtual public sdfCommon
{
public:
    sdfThing(std::string _label = "", std::string _description = "",
            sdfCommon *_reference = NULL, std::vector<sdfCommon*> _required = {},
            std::vector<sdfThing*> _things = {}, std::vector<sdfObject*> _objects = {},
            sdfThing *_parentThing = NULL);
    virtual ~sdfThing();
    // setters
    void setInfo(sdfInfoBlock *_info);
    void setNamespace(sdfNamespaceSection *_ns);
    void addThing(sdfThing *thing);
    void addObject(sdfObject *object);
    void setParentThing(sdfThing *parentThing);
    // getters
    sdfInfoBlock* getInfo() const;
    sdfNamespaceSection* getNamespace() const;
    std::vector<sdfThing*> getThings() const;
    std::vector<sdfObject*> getObjects() const;
    sdfThing* getParentThing() const;
    // parsing
    std::string generateReferenceString();
    nlohmann::json thingToJson(nlohmann::json prefix, bool print_info_namespace = false);
    std::string thingToString(bool print_info_namespace = true);
    void thingToFile(std::string path);
    sdfThing* jsonToThing(nlohmann::json input); // TODO: return void?
    sdfThing* jsonToNestedThing(nlohmann::json input);
    sdfThing* fileToThing(std::string path);

private:
    sdfInfoBlock *info;
    sdfNamespaceSection *ns;
    std::vector<sdfThing*> childThings;
    std::vector<sdfObject*> childObjects;
    sdfThing *parent;
};

/*
 * Products may be composed of Objects and Things at the high level,
 * and may include their own definitions of Properties, Actions, and
 * Events that can be used to extend or complete the included Object
 * definitions. Product definitions may set optional defaults and
 * constant values for specific use cases, for example units, range,
 * and scale settings for properties, or available parameters for Actions
 */
class sdfProduct : sdfThing
{
    // ???
};

#endif

