#include "sdf.hpp"

map<string, sdfCommon*> existingDefinitons;

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
			|| str == "int16" || str == "unint16"
			|| str == "int32" || str == "uint32"
			|| str == "int64" || str == "unint64")
		return json_integer;
	else if (str == "array")
		return json_array;

	return json_type_undef;
}

sdfCommon* refToCommon(string ref)
{
	/*
	smatch sm;
	regex split_regex("#(/(.*))+");
	regex_match(ref, sm, split_regex);
	cout << sm[2] << endl;
	*/
	regex sameNsRegex("#/.*");
	if (!regex_match(ref, sameNsRegex))
	{
		cerr << "Definition from different namespace could not be loaded"
				<< endl;
		return NULL;
	}
	else if (existingDefinitons[ref] != NULL)
	{
		cout<<"refToCommon: "<<existingDefinitons[ref]->getLabel()<<endl;
		return existingDefinitons[ref];
	}
	else
	{
		cerr << "refToCommon(): definition does not exist" << endl;
		return NULL;
	}
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

sdfCommon::sdfCommon(string _label, string _description, sdfCommon *_reference,
		vector<sdfCommon*> _required)
			: description(_description), label(_label), reference(_reference),
			  required(_required)
{
	//this->parent = NULL;
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
	if (this->getReference() != NULL)
	{
		prefix["sdfRef"] = this->getReference()->generateReferenceString();
		cout << this->reference->generateReferenceString()<<endl;
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

sdfObjectElement::sdfObjectElement(string _label, string _description,
		sdfCommon *_reference, vector<sdfCommon*> _required,
		sdfObject *_parentObject)
			: sdfCommon(_label, _description, _reference, _required),
			  parent(_parentObject)
{
}

sdfObject* sdfObjectElement::getParentObject()
{
	return parent;
}

void sdfObjectElement::setParentObject(sdfObject *parentObject)
{
	this->parent = parentObject;
}

string sdfObjectElement::generateReferenceString()
{
	if (this->getParentObject() == NULL)
	{
		cerr << "generateReferenceString(): "
				<< this->getLabel() + " has no assigned parent object."
				<< endl;
		return "";
	}
	else
		return this->parent->generateReferenceString() + "/";
			//+ this->getLabel();
}

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
	cout << "info to json" << endl;
	if (this->getTitle() != "")
		prefix["info"]["title"] = this->getTitle();
	if (this->getVersion() != "")
		prefix["info"]["version"] = this->getVersion();
	if (this->getCopyright() != "")
		prefix["info"]["copyright"] = this->getCopyright();
	if (this->getLicense() != "")
		prefix["info"]["license"] = this->getLicense();
	return prefix;
}

sdfNamespaceSection::sdfNamespaceSection(map<string, string> _namespaces,
		string _default_ns)
			: namespaces(_namespaces), default_ns(_default_ns)
{}


map<string, string> sdfNamespaceSection::getNamespaces()
{
	return namespaces;
}

string sdfNamespaceSection::getDefaultNamespace()
{
	return default_ns;
}

json sdfNamespaceSection::namespaceToJson(json prefix)
{
	cout << "ns to json" << endl;
	for (auto it : this->namespaces)
	{
		prefix["namespace"][it.first] = it.second;
	}
	if (this->default_ns != "")
		prefix["defaultNamespace"] = this->default_ns;
	return prefix;
}

sdfData::sdfData() : sdfData("", "", "")
{}

sdfData::sdfData(string _label, string _description, string _type,
		sdfCommon *_reference, vector<sdfCommon*> _required,
		sdfCommon *_parentCommon)
			: sdfCommon(_label, _description, _reference, _required),
			  parent(_parentCommon)
{
	simpleType = stringToJsonDType(_type);
	if (simpleType == json_type_undef)
		derType = _type;
	else
		derType = "";
	this->setParentCommon(_parentCommon);

	readable = NAN;
	writable = NAN;
	observable = NAN;
	nullable = NAN;
	subtype = sdf_subtype_undef;
	format = json_format_undef;
	constDefined = false;
	defaultDefined = false;
	constantBool = false;
	defaultBool = false;
	constantNumber = NAN;
	defaultNumber = NAN;
	constantInt = -1;
	defaultInt = -1;
	exclusiveMaximum_bool = false;
	exclusiveMinimum_bool = false;
	minLength = NAN;
	maxLength = NAN;
	minimum = NAN;
	maximum = NAN;
	multipleOf = NAN;
	exclusiveMinimum_number = NAN;
	exclusiveMaximum_number = NAN;
	pattern = "";
	format = json_format_undef;
	maxItems = NAN;
	minItems = NAN;
	uniqueItems = NAN;
	item_constr = NULL;
	units = "";
	scaleMinimum = NAN;
	scaleMaximum = NAN;
	contentFormat = "";
}

sdfData::sdfData(string _label, string _description, jsonDataType _type,
		sdfCommon *_reference, vector<sdfCommon*> _required,
		sdfCommon *_parentCommon)
		: sdfData(_label, _description, jsonDTypeToString(_type), _reference,
		_required, _parentCommon)
{}

void sdfData::setNumberData(float _constant,
		float _default, float _min, float _max, float _multipleOf, vector<float> _enum)
{
	if (this->simpleType != json_number && simpleType != json_type_undef)
	{
		cerr << this->getLabel() + " cannot be instantiated as type number"
				+ " because it already has a different type. "
				+ "Please use setSimpType() first to reset it." << endl;
		return;
	}
	if (_min > _max)
		cerr << "Selected minimum value is greater than selected "
				"maximum value: " << _min << " > " << _max << endl;
	this->setSimpType(json_number);
	enumNumber = _enum;
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
		cerr << this->getLabel() + " cannot be instantiated as type string"
				+ " because it already has a different type. "
				+ "Please use setSimpType() first to reset it." << endl;
		return;
	}
	this->setSimpType(json_string);
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
		bool defineDefault, vector<bool> _enum)
{
	if (simpleType != json_boolean && simpleType != json_type_undef)
	{
		cerr << this->getLabel() + " cannot be instantiated as type boolean"
				+ " because it already has a different type. "
				+ "Please use setSimpType() first to reset it." << endl;
		return;
	}
	this->setSimpType(json_boolean);
	enumBool = _enum;
	constDefined = defineConst;
	constantBool = _constant;
	defaultDefined = defineDefault;
	defaultBool = _default;
}

void sdfData::setIntData(int _constant, bool defineConst, int _default,
		bool defineDefault, float _min, float _max, vector<int> _enum)
{
	if (simpleType != json_integer && simpleType != json_type_undef)
	{
		cerr << this->getLabel() + " cannot be instantiated as type integer"
				+ " because it already has a different type. "
				+ "Please use setSimpType() first to reset it." << endl;
		return;
	}
	if (_min > _max)
		cerr << "Selected minimum value is greater than selected "
				"maximum value: " << _min << " > " << _max << endl;
	this->setSimpType(json_integer);
	enumInt = _enum;
	constDefined = defineConst;
	constantInt = _constant;
	defaultDefined = defineDefault;
	defaultInt = _default;
	minimum = _min;
	maximum = _max;
}

void sdfData::setArrayData(float _minItems, float _maxItems,
		bool _uniqueItems, string item_type, sdfCommon *ref)
{
	if (simpleType != json_array && simpleType != json_type_undef)
	{
		cerr << this->getLabel() + " cannot be instantiated as type array"
				+ " because it already has a different type. "
				+ "Please use setSimpType() first to reset it." << endl;
		return;
	}
	if (item_type == "array")
	{
		cerr << "setArrayData(): array as item type not allowed" << endl;
		return;
	}
	this->setSimpType(json_array);
	minItems = _minItems;
	maxItems = _maxItems;
	uniqueItems = _uniqueItems;

	item_constr = new sdfData("", "", item_type, ref);
}

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
	if (!constDefined)
		cerr << "Constant value of sdfData " + this->getLabel()
		+ " was queried but is undefined" << endl;
	return constantBool;
}

int sdfData::getConstantInt()
{
	if (!constDefined)
		cerr << "Constant value of sdfData " + this->getLabel()
		+ " was queried but is undefined" << endl;
	return constantInt;
}

float sdfData::getConstantNumber()
{
	if (!constDefined)
		cerr << "Constant value of sdfData " + this->getLabel()
		+ " was queried but is undefined" << endl;
	return constantNumber;
}

string sdfData::getConstantString()
{
	if (!constDefined)
		cerr << "Constant value of sdfData " + this->getLabel()
		+ " was queried but is undefined" << endl;
	return constantString;
}

string sdfData::getContentFormat()
{
	return contentFormat;
}

bool sdfData::getDefaultBool()
{
	if (!defaultDefined)
		cerr << "Default value of sdfData " + this->getLabel()
		+ " was queried but is undefined" << endl;
	return defaultBool;
}

int sdfData::getDefaultInt()
{
	if (!defaultDefined)
		cerr << "Default value of sdfData " + this->getLabel()
		+ " was queried but is undefined" << endl;
	return defaultInt;
}

float sdfData::getDefaultNumber()
{
	if (!defaultDefined)
		cerr << "Default value of sdfData " + this->getLabel()
		+ " was queried but is undefined" << endl;
	return defaultNumber;
}

string sdfData::getDefaultString()
{
	if (!defaultDefined)
		cerr << "Default value of sdfData " + this->getLabel()
		+ " was queried but is undefined" << endl;
	return defaultString;
}

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
	if (this->simpleType == json_type_undef)
		return jsonDTypeToString(simpleType);
	else
		return this->derType;
}

jsonDataType sdfData::getSimpType()
{
	return simpleType;
}

bool sdfData::getUniqueItems()
{
	return uniqueItems;
}

string sdfData::getUnits()
{
	return units;
}

string sdfData::generateReferenceString()
{
	if (this->getParentCommon() == NULL)
	{
		cerr << "sdfData object " + this->getLabel()
				+ " has no assigned parent object. Possibly it was only"
				+ " added as an output or input data reference and not"
				+ " as a datatype definition."
				<< endl;
		return "";
	}
	else
		return this->getParentCommon()->generateReferenceString()
				+ "/sdfData/" + this->getLabel();
}

json sdfData::dataToJson(json prefix)
{
	json data;
	data = this->commonToJson(data);

	// TODO: really only print sdfRef when it is given?
	// No, because extensions of a referenced element are possible
	//if (this->getReference() == NULL)
	{
		if (this->getUnits() != "")
			data["units"] = this->getUnits();
		if (this->getSubtype() == sdf_byte_string)
			data["subtype"] = "byte-string";
		else if (this->getSubtype() == sdf_unix_time)
				data["subtype"] = "unix-time";
		// TODO: what exactly is the content format??
		if (this->getContentFormat() != "")
			data["contentFormat"] = this->getContentFormat();
		// TODO: are these printed at all?
		if (!this->getReadable())
			data["readable"] = this->getReadable();
		if (!this->getWritable())
			data["writable"] = this->getWritable();
		if (!this->getObservable())
			data["observable"] = this->getObservable();
		if (!this->getNullable())
			data["nullable"] = this->getNullable();

		// TODO: scaleMinimum and scaleMaximum (where do they go?)
		// TODO: exclusiveMinimum and exclusiveMaximum (bool and float)

		switch (simpleType)
		{
		case json_type_undef:
			if (derType != "")
				data["type"] = derType;
			break;
		case json_number:
			if (this->derType  == "")
				data["type"] = "number";
			if (constDefined)
				data["const"] = this->getConstantNumber();
			if (defaultDefined)
				data["default"] = this->getDefaultNumber();
			if (!isnan(this->getMinimum()))
				data["minimum"] = this->getMinimum();
			if (!isnan(this->getMaximum()))
				data["maximum"] = this->getMaximum();
			if (!isnan(this->getMultipleOf()))
				data["multipleOf"] = this->getMultipleOf();
			if (!this->getEnumNumber().empty())
				data["enum"] = this->getEnumNumber();
			break;
		case json_string:
			if (this->derType  == "")
				data["type"] = "string";
			if (!this->getEnumString().empty())
				data["enum"] = this->getEnumString();
			if (constDefined)
				data["const"] = this->getConstantString();
			if (defaultDefined)
				data["default"] = this->getDefaultString();
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
			break;
		case json_boolean:
			if (this->derType  == "")
				data["type"] = "boolean";
			if (this->constDefined)
				data["const"] = this->getConstantBool();
			if (this->defaultDefined)
				data["default"] = this->getDefaultBool();
			if (!this->getEnumBool().empty())
				data["enum"] = this->getEnumBool();
			break;
		case json_integer:
			if (this->derType  == "")
				data["type"] = "integer";
			if (this->constDefined)
				data["const"] = this->getConstantInt();
			if (this->defaultDefined)
				data["default"] = this->getDefaultInt();
			if (!isnan(this->getMinimum()))
				data["minimum"] = (int)this->getMinimum();
			if (!isnan(this->getMaximum()))
				data["maximum"] = (int)this->getMaximum();
			if (!this->getEnumInt().empty())
				data["enum"] = this->getEnumInt();
			break;
		case json_array:
			if (this->derType  == "")
				data["type"] = "array";
			if (!isnan(this->getMinItems()))
				data["minItems"] = this->getMinItems();
			if (!isnan(this->getMaxItems()))
				data["maxItems"] = this->getMaxItems();
			if (this->getUniqueItems())
				data["uniqueItems"] = this->getUniqueItems();
			if (this->item_constr != NULL)
			{
				json tmpJson = data;
				data["items"] = item_constr->dataToJson(tmpJson["items"])["sdfData"][""];
			}
			break;
		}
	}
	prefix["sdfData"][this->getLabel()] = data;
	if (this->getLabel() == "temperature")
		cout << data.dump(4)<<endl;

	return prefix;
}

sdfEvent::sdfEvent(string _label, string _description, sdfCommon *_reference,
		vector<sdfCommon*> _required, sdfObject *_parentObject,
		vector<sdfData*> _outputData, vector<sdfData*> _datatypes)
			: sdfObjectElement(_label, _description, _reference, _required),
			  sdfCommon(_label, _description, _reference, _required),
			  outputData(_outputData), datatypes(_datatypes)
{
	this->setParentObject(_parentObject);
}

void sdfEvent::addOutputData(sdfData *outputData)
{
	this->outputData.push_back(outputData);
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

vector<sdfData*> sdfEvent::getOutputData()
{
	return this->outputData;
}

string sdfEvent::generateReferenceString()
{
	return this->sdfObjectElement::generateReferenceString()
		+ "sdfEvent/" + this->getLabel();
}

json sdfEvent::eventToJson(json prefix)
{
	prefix["sdfEvent"][this->getLabel()]
					 = this->commonToJson(prefix["sdfEvent"][this->getLabel()]);
	for (sdfData *i : this->getDatatypes())
	{
		prefix["sdfEvent"][this->getLabel()]
					 = i->dataToJson(prefix["sdfEvent"][this->getLabel()]);
	}
	for (sdfData *i : this->getOutputData())
	{
		prefix["sdfEvent"][this->getLabel()]["sdfOutputData"][i->getLabel()]
			= i->commonToJson(prefix["sdfEvent"][this->getLabel()]["sdfOutputData"][i->getLabel()]);
		//prefix["sdfEvent"][this->getLabel()]["sdfOutputData"][i->getLabel()]["sdfRef"]
		//			 = i->generateReferenceString();
	}
	return prefix;
}

void sdfAction::addInputData(sdfData *inputData)
{
	this->inputData.push_back(inputData);
}

void sdfAction::addRequiredInputData(sdfData *requiredInputData)
{
	this->requiredInputData.push_back(requiredInputData);
}

void sdfAction::addOutputData(sdfData *outputData)
{
	this->outputData.push_back(outputData);
}

void sdfAction::addDatatype(sdfData *datatype)
{
	this->datatypes.push_back(datatype);
	datatype->setParentCommon((sdfCommon*)this);
}

sdfAction::sdfAction(string _label, string _description, sdfCommon *_reference,
		vector<sdfCommon*> _required, sdfObject *_parentObject,
		vector<sdfData*> _inputData, vector<sdfData*> _requiredInputData,
		vector<sdfData*> _outputData, vector<sdfData*> _datatypes)
			: sdfCommon(_label, _description, _reference, _required),
			  inputData(_inputData), requiredInputData(_requiredInputData),
			  outputData(_outputData), datatypes(_datatypes)
{
	this->setParentObject(_parentObject);
}

vector<sdfData*> sdfAction::getInputData()
{
	return this->inputData;
}

vector<sdfData*> sdfAction::getRequiredInputData()
{
	return this->requiredInputData;
}

vector<sdfData*> sdfAction::getOutputData()
{
	return this->outputData;
}

vector<sdfData*> sdfAction::getDatatypes()
{
	return this->datatypes;
}

string sdfAction::generateReferenceString()
{
	return this->sdfObjectElement::generateReferenceString()
		+ "sdfAction/" + this->getLabel();
}

json sdfAction::actionToJson(json prefix)
{
	prefix["sdfAction"][this->getLabel()]
			= this->commonToJson(prefix["sdfAction"][this->getLabel()]);

	for (sdfData *i : this->getDatatypes())
	{
		prefix["sdfAction"][this->getLabel()]
					 = i->dataToJson(prefix["sdfAction"][this->getLabel()]);
	}
	for (sdfData *i : this->getInputData())
	{
		prefix["sdfAction"][this->getLabel()]["sdfInputData"][i->getLabel()]["sdfRef"]
							 = i->generateReferenceString();
	}
	for (sdfData *i : this->getRequiredInputData())
		{
			prefix["sdfAction"][this->getLabel()]["sdfRequiredInputData"][i->getLabel()]["sdfRef"]
								 = i->generateReferenceString();
		}
	for (sdfData *i : this->getOutputData())
	{
		prefix["sdfAction"][this->getLabel()]["sdfOutputData"][i->getLabel()]["sdfRef"]
					 = i->generateReferenceString();
	}
	return prefix;
}

sdfProperty::sdfProperty(string _label, string _description,
		jsonDataType _type, sdfCommon *_reference,
		vector<sdfCommon*> _required, sdfObject *_parentObject)
		: sdfData(_label, _description, _type, _reference, _required),
		  sdfObjectElement(_label, _description, _reference, _required),
		  sdfCommon(_label, _description, _reference, _required)
{
	this->setParentObject(_parentObject);
	this->setParentCommon(NULL);
}

string sdfProperty::generateReferenceString()
{
	return this->sdfObjectElement::generateReferenceString()
		+ "sdfProperty/" + this->getLabel();
}

json sdfProperty::propertyToJson(json prefix)
{
	json output = prefix;
	output =  this->dataToJson(output);
	prefix["sdfProperty"][this->getLabel()]
						  = output["sdfData"][this->getLabel()];
	return prefix;
}

sdfObject::sdfObject(string _label, string _description, sdfCommon *_reference,
		vector<sdfCommon*> _required, vector<sdfProperty*> _properties,
		vector<sdfAction*> _actions, vector<sdfEvent*> _events,
		vector<sdfData*> _datatypes, sdfThing *_parentThing)
			: sdfCommon(_label, _description, _reference, _required),
			  properties(_properties), actions(_actions), events(_events),
			  datatypes(_datatypes)
{
	this->setParentThing(_parentThing);
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

sdfThing* sdfObject::getParentThing()
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

	prefix["sdfObject"][this->getLabel()]
					= this->commonToJson(prefix["sdfObject"][this->getLabel()]);

	for (sdfData *i : this->getDatatypes())
	{
		prefix["sdfObject"][this->getLabel()]
					= i->dataToJson(prefix["sdfObject"][this->getLabel()]);
	}
	for (sdfProperty *i : this->getProperties())
	{
		prefix["sdfObject"][this->getLabel()]
					= i->propertyToJson(prefix["sdfObject"][this->getLabel()]);
	}
	for (sdfAction *i : this->getActions())
	{
		prefix["sdfObject"][this->getLabel()]
					= i->actionToJson(prefix["sdfObject"][this->getLabel()]);
	}
	for (sdfEvent *i : this->getEvents())
	{
		prefix["sdfObject"][this->getLabel()]
					= i->eventToJson(prefix["sdfObject"][this->getLabel()]);
	}

	return prefix;
}

string sdfObject::objectToString(bool print_info_namespace)
{
	json json_output;
	return this->objectToJson(json_output, print_info_namespace).dump(4);
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
}

string sdfObject::generateReferenceString()
{
	if (this->parent != NULL)
		return this->parent->generateReferenceString() + "/sdfObject/"
				+ this->getLabel();
	else
		return "#/sdfObject/" + this->getLabel();
}

sdfThing::sdfThing(string _label, string _description, sdfCommon *_reference,
		vector<sdfCommon*> _required, vector<sdfThing*> _things,
		vector<sdfObject*> _objects, sdfThing *_parentThing)
			: sdfCommon(_label, _description, _reference, _required),
			  childThings(_things)//, childObjects(_objects)
{
	this->setParentThing(_parentThing);
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
	if (this->getLabel() != "")
		prefix["sdfThing"][this->getLabel()]
				= this->commonToJson(prefix["sdfThing"][this->getLabel()]);
	for (sdfThing *i : this->childThings)
	{
		prefix["sdfThing"][this->getLabel()] = i->thingToJson(prefix["sdfThing"][this->getLabel()], false);
	}

	for (sdfObject *i : this->childObjects)
	{
		prefix["sdfThing"][this->getLabel()] = i->objectToJson(prefix["sdfThing"][this->getLabel()], false);
	}
	return prefix;
}

string sdfThing::thingToString(bool print_info_namespace)
{
	json json_output;
	return this->thingToJson(json_output, print_info_namespace).dump(4);
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
}

sdfThing* sdfThing::getParentThing() const
{
	return parent;
}

void sdfThing::setParentThing(sdfThing *parentThing)
{
	this->parent = parentThing;
}

string sdfThing::generateReferenceString()
{
	if (this->parent != NULL)
		return this->parent->generateReferenceString() + "/"
				+ this->getLabel();
	else
		return "#/sdfThing/" + this->getLabel();
}

void sdfCommon::jsonToCommon(json input)
{
	for (auto it : input.items())
	{
		cout << "jsonToCommon: " << it.key() << endl;
		if (it.key() == "label")
			this->setLabel(correctValue(it.value()));
		else if (it.key() == "description")
			this->setDescription(it.value());
		else if (it.key() == "sdfRef")
			this->reference = refToCommon(correctValue(it.value()));
		else if (it.key() == "sdfRequired")
			for (auto jt : it.value())
				if (refToCommon(jt) != NULL)
					this->addRequired(refToCommon(jt));
	}
	existingDefinitons[this->generateReferenceString()] = this;
	cout << "jsonToCommon: " << this->generateReferenceString() << endl;
}

sdfData* sdfData::jsonToData(json input)
{
	this->jsonToCommon(input);
	for (json::iterator it = input.begin(); it != input.end(); ++it)
	{
		if (it.key() == "type" && !it.value().empty())
		{
			this->setType((string)input["type"]);
		}
		else if (it.key() == "enum" && !it.value().empty())
		{
			if (it.value().is_array())
			{
				for (auto i : it.value())
				{
					if (i.is_number_integer())
						this->enumInt.push_back(i);
					if (i.is_number())
						this->enumNumber.push_back(i);
					else if (i.is_string())
						this->enumString.push_back(i);
					else if (i.is_boolean())
						this->enumBool.push_back(i);
					else if (i.is_array())
						; // TODO: fix this?
				}
			}
		}
		else if (it.key() == "const" && !it.value().empty())
		{
			this->constDefined = true;
			if (it.value().is_number_integer())
				this->constantInt = it.value();
			if (it.value().is_number())
				this->constantNumber = it.value();
			else if (it.value().is_string())
				this->constantString = it.value();
			else if (it.value().is_boolean())
				this->constantBool = it.value();
			else if (it.value().is_array())
				;// TODO: fix this
		}
		else if (it.key() == "default" && !it.value().empty())
		{
			this->defaultDefined = true;
			if (it.value().is_number_integer())
				this->defaultInt = it.value();
			if (it.value().is_number())
				this->defaultNumber = it.value();
			else if (it.value().is_string())
				this->defaultString = it.value();
			else if (it.value().is_boolean())
				this->defaultBool = it.value();
			else if (it.value().is_array())
				;// TODO: fix this
		}
		else if (it.key() == "minimum" && !it.value().empty())
		{
			this->minimum = it.value();
		}
		else if (it.key() == "maximum" && !it.value().empty())
		{
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
		}
		else if (it.key() == "items" && !it.value().empty())
		{
			this->item_constr = new sdfData();
			this->item_constr->jsonToData(input["items"]);
		}
		else if (it.key() == "units" && !it.value().empty())
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
		}
		else if (it.key() == "writable" && !it.value().empty())
		{
			this->writable = it.value();
		}
		else if (it.key() == "observable" && !it.value().empty())
		{
			this->observable = it.value();
		}
		else if (it.key() == "nullable" && !it.value().empty())
		{
			this->nullable = it.value();
		}
		else if (it.key() == "contentFormat" && !it.value().empty())
		{
			// TODO: ??
		}
		else if (it.key() == "subtype" && !it.value().empty())
		{
			if (it.value() == "byte-string")
				this->subtype = sdf_byte_string;
			else if (it.value() == "unix-time")
				this->subtype = sdf_unix_time;
			else
				this->subtype = sdf_subtype_undef;
		}
	}
	return this;
}

sdfEvent* sdfEvent::jsonToEvent(json input)
{
	this->jsonToCommon(input);
	for (auto it : input.items())
	{
		if (it.key() == "sdfOutputData" && !it.value().empty())
		{
			for (json::iterator jt = it.value().begin(); jt != it.value().end(); ++jt)
			{
				sdfData *data = new sdfData();
				data->setLabel(correctValue(jt.key()));
				data->jsonToData(input["sdfOutputData"][jt.key()]);
				this->addOutputData(data);
			}
		}
		else if (it.key() == "sdfData" && !it.value().empty())
		{
			for (json::iterator jt = it.value().begin(); jt != it.value().end(); ++jt)
			{
				sdfData *childData = new sdfData();
				this->addDatatype(childData);
				childData->jsonToData(input["sdfData"][jt.key()]);
			}
		}
	}
	return this;
}

sdfAction* sdfAction::jsonToAction(json input)
{
	this->jsonToCommon(input);
	for (json::iterator it = input.begin(); it != input.end(); ++it)
	{
		if (it.key() == "sdfInputData" && !it.value().empty())
		{
			for (json::iterator jt = it.value().begin(); jt != it.value().end(); ++jt)
			{
				sdfData *refData = new sdfData();
				this->addInputData(refData);
			}
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
		{
			for (json::iterator jt = it.value().begin(); jt != it.value().end(); ++jt)
			{
				sdfData *refData = new sdfData();
				this->addOutputData(refData);
			}
		}
		else if (it.key() == "sdfData" && !it.value().empty())
		{
			for (json::iterator jt = it.value().begin(); jt != it.value().end(); ++jt)
			{
				sdfData *childData = new sdfData();
				this->addDatatype(childData);
				childData->jsonToData(input["sdfData"][jt.key()]);
			}
		}
	}
	return this;
}

sdfProperty* sdfProperty::jsonToProperty(json input)
{
	this->jsonToCommon(input);
	this->jsonToData(input);
	return this;
}

sdfObject* sdfObject::jsonToObject(json input)
{
	this->jsonToCommon(input);
	for (json::iterator it = input.begin(); it != input.end(); ++it)
	{
		cout << "jsonToObject: " << it.key() << endl;
		if (it.key() == "info" && !it.value().empty())
		{
			this->setInfo(new sdfInfoBlock());
			this->info->jsonToInfo(input["info"]);
		}
		else if (it.key() == "namespace" && !it.value().empty())
		{
			this->setNamespace(new sdfNamespaceSection());
			this->ns->jsonToNamespace(input);
		}
		// for first level
		else if (it.key() == "sdfObject" && !it.value().empty())
		{
			for (json::iterator jt = it.value().begin(); jt != it.value().end(); ++jt)
				this->jsonToObject(input["sdfObject"][jt.key()]);
		}
		else if (it.key() == "sdfData" && !it.value().empty())
		{
			for (json::iterator jt = it.value().begin(); jt != it.value().end(); ++jt)
			{
				sdfData *childData = new sdfData();
				this->addDatatype(childData);
				childData->setLabel(correctValue(jt.key()));
				childData->jsonToData(input["sdfData"][jt.key()]);
			}
		}
		else if (it.key() == "sdfProperty" && !it.value().empty())
		{
			for (json::iterator jt = it.value().begin(); jt != it.value().end(); ++jt)
			{
				sdfProperty *childProperty = new sdfProperty();
				this->addProperty(childProperty);
				childProperty->setLabel(correctValue(jt.key()));
				childProperty->jsonToProperty(input["sdfProperty"][jt.key()]);

				//childProperty->setLabel("hi_oh");//jt.key());
			}
		}
		else if (it.key() == "sdfAction" && !it.value().empty())
		{
			for (json::iterator jt = it.value().begin(); jt != it.value().end(); ++jt)
			{
				sdfAction *childAction = new sdfAction();
				this->addAction(childAction);
				childAction->setLabel(correctValue(jt.key()));
				childAction->jsonToAction(input["sdfAction"][jt.key()]);
			}
		}
		else if (it.key() == "sdfEvent" && !it.value().empty())
		{
			for (json::iterator jt = it.value().begin(); jt != it.value().end(); ++jt)
			{
				sdfEvent *childEvent =  new sdfEvent();
				this->addEvent(childEvent);
				// TODO: with label set this way, a label will be printed
				// even though there was just a "title" in the original
				childEvent->setLabel(correctValue(jt.key()));
				childEvent->jsonToEvent(input["sdfEvent"][jt.key()]);
			}
		}
		else if (it.key() == "sdfThing")
		{
			cerr << "jsonToObject(): incorrect sdfObject (sdfThing found)";
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
	return this;
}

sdfObject* sdfObject::fileToObject(string path)
{
	json json_input;
	ifstream input(path);
	if (input)
	{
		input >>  json_input;
		input.close();
	}
	else
		cerr << "Error opening file" << endl;
	return this->jsonToObject(json_input);
}

/*
 * This has to be an extra function because it is otherwise not possible
 * to distinguish whether this is a first-level Thing or a nested thing
 */
sdfThing* sdfThing::jsonToNestedThing(json input)
{
	this->jsonToCommon(input);
	for (json::iterator it = input.begin(); it != input.end(); ++it)
	{
	    if (it.key() == "sdfThing" && !it.value().empty())
		{
			for (json::iterator jt = it.value().begin(); jt != it.value().end(); ++jt)
			{
				sdfThing *childThing = new sdfThing();
				this->addThing(childThing);
				childThing->jsonToNestedThing(input["sdfThing"][jt.key()]);
			}
		}
		else if (it.key() == "sdfObject" && !it.value().empty())
		{
			for (json::iterator jt = it.value().begin(); jt != it.value().end(); ++jt)
			{
				sdfObject *childObject = new sdfObject();
				this->addObject(childObject);
				childObject->jsonToObject(input["sdfObject"][jt.key()]);
			}
		}
	}
	return this;
}

sdfThing* sdfThing::jsonToThing(json input)
{
	this->jsonToCommon(input);
	sdfObject *childObject;

	for (json::iterator it = input.begin(); it != input.end(); ++it)
	{
	    cout << "jsonToThing: " << it.key() << endl;
		if (it.key() == "info" && !it.value().empty())
		{
			this->setInfo(new sdfInfoBlock());
			this->info->jsonToInfo(input["info"]);
		}
		else if (it.key() == "namespace" && !it.value().empty())
		{
			this->setNamespace(new sdfNamespaceSection());
			this->ns->jsonToNamespace(input);
		}
		else if (it.key() == "defaultNamespace" && !it.value().empty())
		{
			this->ns->jsonToNamespace(input["namespace"]);
		}
		else if (it.key() == "sdfThing" && !it.value().empty())
		{
			for (json::iterator jt = it.value().begin(); jt != it.value().end(); ++jt)
			{
				//sdfThing *childThing;
				//childThing->jsonToThing(input["sdfThing"][jt.key()]);
				//this->addThing(childThing);
				this->jsonToNestedThing(input["sdfThing"][jt.key()]);
			}
		}
		else if (it.key() == "sdfObject" && !it.value().empty())
		{
			for (json::iterator jt = it.value().begin(); jt != it.value().end(); ++jt)
			{
				childObject = new sdfObject();
				this->addObject(childObject);
				childObject->jsonToObject(input["sdfObject"][jt.key()]);
			}
		}
	}
	return this;
}

sdfThing* sdfThing::fileToThing(string path)
{
	json json_input;
	ifstream input(path);
	if (input)
	{
		input >>  json_input;
		input.close();
	}
	else
		cerr << "Error opening file" << endl;
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
	this->simpleType = _type;
}

void sdfData::setType(string _type)
{
	if (stringToJsonDType(_type) == json_type_undef)
		this->derType = _type;
	//else
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
}

void sdfData::setConstantInt(int constantInt)
{
	this->constantInt = constantInt;
	this->constDefined = true;
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
}

void sdfData::setDefaultInt(int defaultInt)
{
	this->defaultInt = defaultInt;
	this->defaultDefined = true;
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

vector<string> sdfData::getConstantArray()
{
	return constantArray;
}

void sdfData::setConstantArray(vector<string> constantArray)
{
	this->constantArray = constantArray;
	this->constDefined = true;
}

vector<string> sdfData::getDefaultArray()
{
	return defaultArray;
}

void sdfData::setDefaultArray(vector<string> defaultArray)
{
	this->defaultArray = defaultArray;
	this->defaultDefined = true;
}

void sdfData::setParentCommon(sdfCommon *parentCommon)
{
	this->parent = parentCommon;
}

sdfCommon* sdfData::getParentCommon()
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
					this->namespaces[jt.key()] = jt.value();
			}
		}
		else if (it.key() == "defaultNamespace" && !it.value().empty())
			this->default_ns = it.value();
	}
	return this;
}
