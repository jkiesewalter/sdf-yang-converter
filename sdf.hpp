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
#include <nlohmann/json.hpp>

// for convenience
using json = nlohmann::json;
using namespace std;

enum jsonDataType
{
	json_number,
	json_string,
	json_boolean,
	json_integer,
	json_array,
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

string jsonDTypeToString(jsonDataType type);
jsonDataType stringToJsonDType(string str);
sdfCommon* refToCommon(string ref);
string correctValue(string val);


// sdfObject, sdfProperty, sdfAction, sdfEvent and sdfData are sdfCommons
class sdfCommon
{
public:
    sdfCommon(string _label = "", string _description = "",
    		sdfCommon *_reference = NULL, vector<sdfCommon*> _required = {});
    // getters
    string getDescription();
    string getLabel();
    sdfCommon* getReference();
    vector<sdfCommon*> getRequired();
	//sdfCommon* getParentCommon();
    // setters
    void setLabel(string _label);
    void setDescription(string dsc);
    void addRequired(sdfCommon *common);
    void setReference(sdfCommon *common);
    //void setParentCommon(sdfCommon *parentCommon);
    // printing
    virtual string generateReferenceString() = 0;
    json commonToJson(json prefix);
    void jsonToCommon(json input);
private:
    string description;
    string label;
    sdfCommon *reference;
    vector<sdfCommon*> required;
	//sdfCommon *parent;
};

class sdfObjectElement : virtual public sdfCommon
{
public:
	sdfObjectElement(string _label = "", string _description = "",
			sdfCommon *_reference = NULL, vector<sdfCommon*> _required = {},
			sdfObject *_parentObject = NULL);
	sdfObject* getParentObject();
	void setParentObject(sdfObject *parentObject);
	string generateReferenceString();
private:
	sdfObject *parent;
};

class sdfInfoBlock
{
public:
    sdfInfoBlock(string _title = "", string _version = "",
                string _copyright = "", string _license= "");
    // getters
    string getTitle();
    string getVersion();
    string getCopyright();
    string getLicense();
    // parsing
    json infoToJson(json prefix);
    sdfInfoBlock* jsonToInfo(json input);
private:
    string title;
    string version;
    string copyright;
    string license;
};

class sdfNamespaceSection
{
public:
    sdfNamespaceSection(map<string, string> _namespaces = {},
    					string _default_ns = "");
    // getters
    map<string, string> getNamespaces();
    string getDefaultNamespace();
    // parsing
    json namespaceToJson(json prefix);
    sdfNamespaceSection* jsonToNamespace(json input);
private:
    map<string, string> namespaces;
    string default_ns;
};

class sdfData : virtual public sdfCommon
{
public:
	sdfData();
    sdfData(string _label,
    		string _description,
			string _type,
			sdfCommon *_reference = NULL,
			vector<sdfCommon*> _required = {},
			sdfCommon *_parentCommon = NULL);
    sdfData(string _label,
			string _description,
			jsonDataType _type,
			sdfCommon *_reference = NULL,
			vector<sdfCommon*> _required = {},
			sdfCommon *_parentCommon = NULL);
    // Setters for member variables (different for types)
    // check which of the sdfData data qualities are designated for which
    // data type (number, string, ...)
    void setNumberData(float _constant = NAN,
    		float _default = NAN,
    		float _min = NAN,
			float _max = NAN,
			float _multipleOf = NAN,
			vector<float> _enum = {});
    // TODO: constant and default should not be set at the same time?
    void setStringData(string _constant = "",
    				   string _default = "",
					   float _minLength = NAN,
					   float _maxLength = NAN,
					   string _pattern = "",
					   jsonSchemaFormat _format = json_format_undef,
					   vector<string> _enum = {});
    void setBoolData(bool _constant = false,
					 bool defineConst = false,
					 bool _default = false,
					 bool defineDefault = false,
					 vector<bool> _enum = {});
    void setIntData(int _constant = -1,
					bool defineConst = false,
					int _default = -1,
					bool defineDefault = false,
					float _min = NAN,
					float _max = NAN,
					vector<int> _enum = {});
    void setArrayData(float _minItems,
					  float _maxItems,
				  	  bool _uniqueItems,
					  string item_type,
					  sdfCommon *ref = NULL);
    void setArrayData(vector<string> enm,
    				  float _minItems = NAN,
					  float _maxItems = NAN,
					  bool _uniqueItems = NAN,
				   	  string item_type = "string",
					  sdfCommon *ref = NULL,
					  float minLength = NAN,
					  float maxLength = NAN,
					  jsonSchemaFormat format = json_format_undef);
    void setArrayData(vector<float> enm,
    				  float _minItems = NAN,
					  float _maxItems = NAN,
					  bool _uniqueItems = NAN,
					  string item_type = "number",
					  sdfCommon *ref = NULL,
					  float min = NAN,
					  float max = NAN);
    void setArrayData(vector<int> enm,
    				  float _minItems = NAN,
					  float _maxItems = NAN,
					  bool _uniqueItems = NAN,
					  string item_type = "integer",
					  sdfCommon *ref = NULL,
					  float min = NAN,
					  float max = NAN);
    // other setters
    void setSimpType(jsonDataType _type);
    void setType(string _type);
    void setType(jsonDataType _type);
    void setDerType(string _type);
    void setUnits(string _units, float _scaleMin = NAN, float _scaleMax = NAN);
    void setReadWrite(bool _readable, bool _writable);
    void setObserveNull(bool _observable, bool _nullable);
    void setFormat(jsonSchemaFormat _format);
    void setSubtype(sdfSubtype _subtype);
	void setMaxItems(float maxItems);
	void setMinItems(float minItems);
	void setPattern(string pattern);
	void setConstantBool(bool constantBool);
	void setConstantInt(int constantInt);
	void setConstantNumber(float constantNumber);
	void setConstantString(string constantString);
	void setDefaultBool(bool defaultBool);
	void setDefaultInt(int defaultInt);
	void setDefaultNumber(float defaultNumber);
	void setDefaultString(string defaultString);
	void setConstantArray(vector<string> constantArray);
	void setDefaultArray(vector<string> defaultArray);
    void setParentCommon(sdfCommon *parentCommon);
    // getters for member variables
    bool getReadable();
    bool getWritable();
	bool getNullable();
	bool getObservable();
	bool getConstantBool();
	int getConstantInt();
	float getConstantNumber();
	string getConstantString();
	string getContentFormat();
	bool getDefaultBool();
	int getDefaultInt();
	float getDefaultNumber();
	string getDefaultString();
	vector<bool> getEnumBool();
	vector<int> getEnumInt();
	vector<float> getEnumNumber();
	vector<string> getEnumString();
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
	string getPattern();
	float getScaleMaximum();
	float getScaleMinimum();
	sdfSubtype getSubtype();
	jsonDataType getSimpType();
	string getType();
	bool getUniqueItems();
	string getUnits();
	vector<string> getConstantArray();
	vector<string> getDefaultArray();
	sdfCommon* getParentCommon();
	// parsing
	string generateReferenceString();
    json dataToJson(json prefix);
    sdfData* jsonToData(json input);

private:
    bool constDefined;
    bool defaultDefined;
    jsonDataType simpleType;
    //vector<jsonDataType> unionTypes;
    string derType;
    // only fill for type number
    vector<float> enumNumber;
    float constantNumber;
    float defaultNumber;
    // only fill for type string
    vector<string> enumString;
    string constantString;
    string defaultString;
    // only fill for type boolean
    vector<bool> enumBool; // does this even make sense?
    bool constantBool;
    bool defaultBool;
    // only fill for type integer
    vector<int> enumInt;
    int constantInt;
    int defaultInt;
    // only fill for type array
    // TODO: find a way to represent not only string arrays as const/default
    vector<string> constantArray;
    vector<string> defaultArray;
    /*vector<vector<auto>> enumArray;
    vector<auto> constantArray;
    vector<auto> defaultArray;*/
    float minimum;
    float maximum;
    float exclusiveMinimum_number; // TODO: I don't get this
    float exclusiveMaximum_number;
    bool exclusiveMinimum_bool;
    bool exclusiveMaximum_bool;
    float multipleOf;
    float minLength;
    float maxLength;
    string pattern; // regex?
    jsonSchemaFormat format;
    float minItems;
    float maxItems;
    bool uniqueItems;
    sdfData *item_constr;
    // SDF-defined data qualities
    string units;
    float scaleMinimum;
    float scaleMaximum;
    bool readable;
    bool writable;
    bool observable;
    bool nullable;
    string contentFormat;
    sdfSubtype subtype;
	sdfCommon *parent;
};

class sdfEvent : virtual public sdfObjectElement
{
public:
    sdfEvent(string _label = "",
			string _description = "",
			sdfCommon *_reference = NULL,
			vector<sdfCommon*> _required = {},
			sdfObject *_parentObject = NULL,
			vector<sdfData*> _outputData = {},
			vector<sdfData*> _datatypes = {});
    // setters
    void addOutputData(sdfData *outputData);
    void addDatatype(sdfData *datatype);
    // getters
    vector<sdfData*> getDatatypes();
    vector<sdfData*> getOutputData();
    // parsing
	string generateReferenceString();
    json eventToJson(json prefix);
    sdfEvent* jsonToEvent(json input);
private:
    vector<sdfData*> outputData;
    vector<sdfData*> datatypes;
};

class sdfAction : virtual public sdfObjectElement
{
public:
    sdfAction(string _label = "",
    		string _description = "",
			sdfCommon *_reference = NULL,
            vector<sdfCommon*> _required = {},
			sdfObject *_parentObject = NULL,
			vector<sdfData*> _inputData = {},
			vector<sdfData*> _requiredInputData = {},
			vector<sdfData*> _outputData = {},
			vector<sdfData*> _datatypes = {});
    // setters
    void addInputData(sdfData* inputData);
    void addRequiredInputData(sdfData* requiredInputData);
    void addOutputData(sdfData* outputData);
    void addDatatype(sdfData *datatype);
    // getters
    vector<sdfData*> getInputData();
    vector<sdfData*> getRequiredInputData();
    vector<sdfData*> getOutputData();
    vector<sdfData*> getDatatypes();
    // parsing
	string generateReferenceString();
    json actionToJson(json prefix);
    sdfAction* jsonToAction(json input);

private:
    vector<sdfData*> inputData;
    vector<sdfData*> requiredInputData;
    vector<sdfData*> outputData;
    vector<sdfData*> datatypes;
};

class sdfProperty : public sdfData, virtual public sdfObjectElement
{
public:
	sdfProperty(string _label = "",
			string _description = "",
			jsonDataType _type = {},
			sdfCommon *_reference = NULL,
			vector<sdfCommon*> _required = {},
			 sdfObject *_parentObject = NULL);
	// parsing
	string generateReferenceString();
    json propertyToJson(json prefix);
    sdfProperty* jsonToProperty(json input);
};

class sdfObject : virtual public sdfCommon
{
public:
    sdfObject(string _label = "", string _description = "",
    		sdfCommon *_reference = NULL, vector<sdfCommon*> _required = {},
			vector<sdfProperty*> _properties = {},
			vector<sdfAction*> _actions = {}, vector<sdfEvent*> _events = {},
			vector<sdfData*> _datatypes = {}, sdfThing *_parentThing = NULL);
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
    vector<sdfProperty*> getProperties();
    vector<sdfAction*> getActions();
    vector<sdfEvent*> getEvents();
    vector<sdfData*> getDatatypes();
	sdfThing* getParentThing();
    // parsing
    string generateReferenceString();
    json objectToJson(json prefix, bool print_info_namespace = true);
    string objectToString(bool print_info_namespace = true);
    void objectToFile(string path);
    sdfObject* jsonToObject(json input);
    sdfObject* fileToObject(string path);

private:
    json actionToJson();
    json eventToJson();
    sdfInfoBlock *info;
    sdfNamespaceSection *ns;
    vector<sdfProperty*> properties;
    vector<sdfAction*> actions;
    vector<sdfEvent*> events;
    vector<sdfData*> datatypes;
    sdfThing *parent;
};

class sdfThing : virtual public sdfCommon
{
public:
    sdfThing(string _label = "", string _description = "",
    		sdfCommon *_reference = NULL, vector<sdfCommon*> _required = {},
			vector<sdfThing*> _things = {}, vector<sdfObject*> _objects = {},
			sdfThing *_parentThing = NULL);
    // setters
    void setInfo(sdfInfoBlock *_info);
    void setNamespace(sdfNamespaceSection *_ns);
    void addThing(sdfThing *thing);
    void addObject(sdfObject *object);
	void setParentThing(sdfThing *parentThing);
    // getters
    sdfInfoBlock* getInfo() const;
    sdfNamespaceSection* getNamespace() const;
    vector<sdfThing*> getThings() const;
    vector<sdfObject*> getObjects() const;
	sdfThing* getParentThing() const;
    // parsing
    string generateReferenceString();
    json thingToJson(json prefix, bool print_info_namespace = false);
    string thingToString(bool print_info_namespace = true);
    void thingToFile(string path);
    sdfThing* jsonToThing(json input); // TODO: return void?
    sdfThing* jsonToNestedThing(json input);
    sdfThing* fileToThing(string path);

private:
    sdfInfoBlock *info;
    sdfNamespaceSection *ns;
    vector<sdfThing*> childThings;
    vector<sdfObject*> childObjects;
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

