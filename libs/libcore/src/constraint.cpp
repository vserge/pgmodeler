/*
# PostgreSQL Database Modeler (pgModeler)
#
# Copyright 2006-2023 - Raphael Araújo e Silva <raphael@pgmodeler.io>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation version 3.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# The complete text of GPLv3 is at LICENSE file on source code root directory.
# Also, you can get the complete GNU General Public License at <http://www.gnu.org/licenses/>
*/

#include "constraint.h"

Constraint::Constraint()
{
	ref_table=nullptr;
	obj_type=ObjectType::Constraint;
	deferrable=false;
	no_inherit=false;
	fill_factor=0;
	index_type=IndexingType::Null;

	attributes[Attributes::PkConstr]="";
	attributes[Attributes::FkConstr]="";
	attributes[Attributes::CkConstr]="";
	attributes[Attributes::UqConstr]="";
	attributes[Attributes::ExConstr]="";
	attributes[Attributes::RefTable]="";
	attributes[Attributes::SrcColumns]="";
	attributes[Attributes::DstColumns]="";
	attributes[Attributes::DelAction]="";
	attributes[Attributes::UpdAction]="";
	attributes[Attributes::Expression]="";
	attributes[Attributes::Type]="";
	attributes[Attributes::ComparisonType]="";
	attributes[Attributes::DeferType]="";
	attributes[Attributes::IndexType]="";
	attributes[Attributes::Deferrable]="";
	attributes[Attributes::Table]="";
	attributes[Attributes::DeclInTable]="";
	attributes[Attributes::Factor]="";
	attributes[Attributes::NoInherit]="";
	attributes[Attributes::Elements]="";
}

Constraint::~Constraint()
{
	columns.clear();
	ref_columns.clear();
}

void Constraint::setConstraintType(ConstraintType constr_type)
{
	this->constr_type=constr_type;
}

void Constraint::setActionType(ActionType action_type, ActionEvent act_event)
{
	if(act_event == DeleteAction)
	{
		setCodeInvalidated(this->del_action != action_type);
		this->del_action=action_type;
	}
	else
	{
		setCodeInvalidated(this->upd_action != action_type);
		this->upd_action=action_type;
	}
}

void Constraint::setExpression(const QString &expr)
{
	setCodeInvalidated(expression != expr);
	expression=expr;
}

bool Constraint::isColumnExists(Column *column, ColumnsId cols_id)
{
	std::vector<Column *>::iterator itr, itr_end;
	bool found=false;

	//Raises an error if the column is not allocated
	if(!column)
		throw Exception(ErrorCode::OprNotAllocatedObject,__PRETTY_FUNCTION__,__FILE__,__LINE__);

	//Gets the iterators from the specified internal list
	if(cols_id==SourceCols)
	{
		itr=columns.begin();
		itr_end=columns.end();
	}
	else
	{
		itr=ref_columns.begin();
		itr_end=ref_columns.end();
	}

	//Tries to find the column  on the internal list
	while(itr!=itr_end && !found)
	{
		found=((*itr)==column);
		itr++;
	}

	return found;
}

bool Constraint::isColumnsExist(std::vector<Column *> columns, ColumnsId cols_id, bool strict_check)
{
	bool is_ref = false;
	unsigned found_cols = 0;

	for(auto &col : columns)
	{
		is_ref = isColumnExists(col, cols_id);
		if(!strict_check && !is_ref) break;
		if(strict_check) found_cols++;
	}

	return (!strict_check && is_ref) ||
					(strict_check && found_cols == getColumnCount(cols_id));
}

bool Constraint::isColumnReferenced(Column *column, bool search_only_ref_cols)
{
	bool found=false;
	std::vector<ExcludeElement>::iterator itr, itr_end;

	if(constr_type == ConstraintType::PrimaryKey ||
			constr_type == ConstraintType::Unique ||
			constr_type == ConstraintType::ForeignKey)
	{
		if(!search_only_ref_cols)
			found=isColumnExists(column, SourceCols);

		if(!found && constr_type==ConstraintType::ForeignKey)
			found=isColumnExists(column, ReferencedCols);
	}
	else if(constr_type==ConstraintType::Exclude)
	{
		//Iterates over the exclude elements
		itr=excl_elements.begin();
		itr_end=excl_elements.end();

		while(itr!=itr_end && !found)
		{
			found=((*itr).getColumn() == column);
			itr++;
		}
	}

	return found;
}

void Constraint::addColumn(Column *column, ColumnsId cols_id)
{
	//Raises an error if the column is not allocated
	if(!column)
		throw Exception(Exception::getErrorMessage(ErrorCode::AsgNotAllocatedColumn)
						.arg(this->getName())
						.arg(BaseObject::getTypeName(ObjectType::Constraint)),
						ErrorCode::AsgNotAllocatedColumn,__PRETTY_FUNCTION__,__FILE__,__LINE__);

	if(constr_type!=ConstraintType::Check)
	{
		//Adds the column only if the column doesn't exists on the internal list
		if(!isColumnExists(column,cols_id))
		{
			if(cols_id==ReferencedCols)
				ref_columns.push_back(column);
			else
			{
				columns.push_back(column);
				setColumnsNotNull(true);
			}

			setCodeInvalidated(true);
		}
	}
}

void Constraint::addColumns(const std::vector<Column *> &cols, ColumnsId cols_id)
{
	try
	{
		if(cols_id == ReferencedCols)
			ref_columns.clear();
		else
			columns.clear();

		for(auto &col : cols)
			addColumn(col, cols_id);
	}
	catch(Exception &e)
	{
		throw Exception(e.getErrorMessage(), e.getErrorCode(), __PRETTY_FUNCTION__, __FILE__, __LINE__, &e);
	}
}

void Constraint::setTablespace(BaseObject *tabspc)
{
	try
	{
		if(tabspc &&
				constr_type!=ConstraintType::PrimaryKey &&
				constr_type!=ConstraintType::Unique)
			throw Exception(ErrorCode::AsgTablespaceInvalidConstraintType,__PRETTY_FUNCTION__,__FILE__,__LINE__);

		BaseObject::setTablespace(tabspc);
	}
	catch(Exception &e)
	{
		throw Exception(e.getErrorMessage(),e.getErrorCode(),__PRETTY_FUNCTION__,__FILE__,__LINE__,&e);
	}
}

void Constraint::setColumnsAttribute(ColumnsId cols_id, unsigned def_type, bool inc_addedbyrel)
{
	std::vector<Column *> *col_vector=nullptr;
	Column *col=nullptr;
	QString str_cols, attrib;
	unsigned i, count;
	bool format=(def_type==SchemaParser::SqlCode);

	if(cols_id==ReferencedCols)
	{
		col_vector=&ref_columns;
		attrib=Attributes::DstColumns;
	}
	else
	{
		col_vector=&columns;
		attrib=Attributes::SrcColumns;
	}

	count=col_vector->size();
	for(i=0; i < count; i++)
	{
		col=col_vector->at(i);

		/* For XML definition the columns added to the constraint
		 through relationship can not be included because they are inserted
		 to the restriction on the time of creation of the relationship from its XML
		 so the parameter 'inc_addedbyrel' can be used to solve this case. */
		if((def_type==SchemaParser::SqlCode) ||
				((def_type==SchemaParser::XmlCode) &&
				 ((inc_addedbyrel && col->isAddedByRelationship()) ||
				  (inc_addedbyrel && !col->isAddedByRelationship()) ||
				  (!inc_addedbyrel && !col->isAddedByRelationship()))))
		{
			str_cols+=col->getName(format);
			str_cols+=',';
		}
	}

	str_cols.remove(str_cols.size()-1,1);
	attributes[attrib]=str_cols;
}

void Constraint::setReferencedTable(BaseTable *tab_ref)
{
	this->ref_table=tab_ref;
}

void Constraint::setDeferralType(DeferralType deferral_type)
{
	this->deferral_type=deferral_type;
}

void Constraint::setDeferrable(bool value)
{
	deferrable=value;
}

void Constraint::setMatchType(MatchType match_type)
{
	this->match_type=match_type;
}

void Constraint::setFillFactor(unsigned factor)
{
	if(factor!=0 && factor < 10) factor=10;
	else if(factor > 100) factor=100;
	fill_factor=factor;
}

void Constraint::setNoInherit(bool value)
{
	no_inherit=value;
}

unsigned Constraint::getFillFactor()
{
	return fill_factor;
}

ConstraintType Constraint::getConstraintType()
{
	return constr_type;
}

ActionType Constraint::getActionType(ActionEvent act_event)
{
	if(act_event == DeleteAction)
		return del_action;

	return upd_action;
}

std::vector<Column *> Constraint::getColumns(ColumnsId cols_id)
{
	return (cols_id == SourceCols ? columns : ref_columns);
}

QString Constraint::getExpression()
{
	return expression;
}

Column *Constraint::getColumn(unsigned col_idx, ColumnsId cols_id)
{
	std::vector<Column *> *col_list=nullptr;

	col_list=(cols_id==SourceCols ? &columns : &ref_columns);

	//Raises an error if the column index is invalid (out of bound)
	if(col_idx>=col_list->size())
		throw Exception(ErrorCode::RefColumnInvalidIndex,__PRETTY_FUNCTION__,__FILE__,__LINE__);

	return col_list->at(col_idx);
}

Column *Constraint::getColumn(const QString &name, ColumnsId cols_id)
{
	bool found=false;
	std::vector<Column *> *col_list=nullptr;
	std::vector<Column *>::iterator itr_col, itr_end_col;

	col_list=(cols_id==SourceCols? &columns : &ref_columns);

	itr_col=col_list->begin();
	itr_end_col=col_list->end();

	while(itr_col!=itr_end_col)
	{
		found=((*itr_col)->getName()==name);
		if(!found) itr_col++;
		else break;
	}

	if(found) return *itr_col;
	else return nullptr;
}

BaseTable *Constraint::getReferencedTable()
{
	return ref_table;
}

unsigned Constraint::getColumnCount(ColumnsId cols_id)
{
	if(cols_id==ReferencedCols)
		return ref_columns.size();

	return columns.size();
}

void Constraint::removeColumns()
{
	setColumnsNotNull(false);
	columns.clear();
	ref_columns.clear();
	setCodeInvalidated(true);
}

void Constraint::removeColumn(const QString &name, ColumnsId cols_id)
{
	std::vector<Column *>::iterator itr, itr_end;
	std::vector<Column *> *cols=nullptr;
	Column *col=nullptr;

	//Gets the column list using the specified internal list type
	if(cols_id==ReferencedCols)
		cols=&ref_columns;
	else
		cols=&columns;

	itr=cols->begin();
	itr_end=cols->end();

	while(itr!=itr_end)
	{
		col=(*itr);

		//Case the column is found
		if(col->getName()==name)
		{
			if(constr_type==ConstraintType::PrimaryKey)
				col->setNotNull(false);

			//Remove its iterator from the list
			cols->erase(itr);
			setCodeInvalidated(true);
			break;
		}
		else itr++;
	}
}

DeferralType Constraint::getDeferralType()
{
	return deferral_type;
}

bool Constraint::isDeferrable()
{
	return deferrable;
}

bool Constraint::isNoInherit()
{
	return no_inherit;
}

bool Constraint::isReferRelationshipAddedColumn()
{
	std::vector<Column *>::iterator itr, itr_end;
	std::vector<ExcludeElement>::iterator itr1, itr1_end;
	Column *col=nullptr;
	bool found=false;

	//First iterates over the source columns list
	itr=columns.begin();
	itr_end=columns.end();

	while(itr!=itr_end && !found)
	{
		col=(*itr);
		//Check if the current column were added by relationship
		found=col->isAddedByRelationship();
		itr++;

		/* Case the source column list is completely iterated steps to
		 the referenced columns list iteration */
		if(itr==itr_end && itr_end!=ref_columns.end() && !found)
		{
			itr=ref_columns.begin();
			itr_end=ref_columns.end();
		}
	}

	//Iterates over the exclude elements
	itr1=excl_elements.begin();
	itr1_end=excl_elements.end();

	while(itr1!=itr1_end && !found)
	{
		col=(*itr1).getColumn();
		found=(col && col->isAddedByRelationship());
		itr1++;
	}

	return found;
}

std::vector<Column *> Constraint::getRelationshipAddedColumns()
{
	Column *column=nullptr;
	std::vector<Column *> cols;
	std::vector<std::vector<Column *> *> lists = { &columns, &ref_columns };

	for(auto &p_lst : lists)
	{
		for(auto &col : (*p_lst))
		{
			if(col->isAddedByRelationship())
				cols.push_back(col);
		}
	}

	for(auto &excl_elem : excl_elements)
	{
		column=excl_elem.getColumn();
		if(column && column->isAddedByRelationship())
			cols.push_back(column);
	}

	return cols;
}

MatchType Constraint::getMatchType()
{
	return match_type;
}


int Constraint::getExcludeElementIndex(ExcludeElement elem)
{
	int idx=0;
	bool found=false;

	while(idx < static_cast<int>(excl_elements.size()) && !found)
	{
		found=(excl_elements[idx]==elem);
		if(!found) idx++;
	}

	return (found ? idx : -1);
}

std::vector<ExcludeElement> Constraint::getExcludeElements()
{
	return excl_elements;
}

void Constraint::addExcludeElements(std::vector<ExcludeElement> &elems)
{
	std::vector<ExcludeElement> elems_bkp=excl_elements;

	try
	{
		excl_elements.clear();

		for(unsigned i=0; i < elems.size(); i++)
			addExcludeElement(elems[i]);
	}
	catch(Exception &e)
	{
		excl_elements = elems_bkp;
		throw Exception(e.getErrorMessage(), e.getErrorCode(),__PRETTY_FUNCTION__,__FILE__,__LINE__, &e);
	}
}

void Constraint::addExcludeElement(ExcludeElement elem)
{
	if(getExcludeElementIndex(elem) >= 0)
		throw Exception(ErrorCode::InsDuplicatedElement,__PRETTY_FUNCTION__,__FILE__,__LINE__);

	if(elem.getExpression().isEmpty() && !elem.getColumn())
		throw Exception(ErrorCode::AsgInvalidExpressionObject,__PRETTY_FUNCTION__,__FILE__,__LINE__);

	excl_elements.push_back(elem);
	setCodeInvalidated(true);
}

void Constraint::addExcludeElement(const QString &expr, Operator *oper, OperatorClass *op_class, bool use_sorting, bool asc_order, bool nulls_first)
{
	try
	{
		ExcludeElement elem;

		//Raises an error if the expression is empty
		if(expr.isEmpty())
			throw Exception(ErrorCode::AsgInvalidExpressionObject,__PRETTY_FUNCTION__,__FILE__,__LINE__);

		//Configures the element
		elem.setExpression(expr);
		elem.setOperatorClass(op_class);
		elem.setOperator(oper);
		elem.setSortingEnabled(use_sorting);
		elem.setSortingAttribute(ExcludeElement::NullsFirst, nulls_first);
		elem.setSortingAttribute(ExcludeElement::AscOrder, asc_order);

		if(getExcludeElementIndex(elem) >= 0)
			throw Exception(ErrorCode::InsDuplicatedElement,__PRETTY_FUNCTION__,__FILE__,__LINE__);

		excl_elements.push_back(elem);
		setCodeInvalidated(true);
	}
	catch(Exception &e)
	{
		throw Exception(e.getErrorMessage(), e.getErrorCode(),__PRETTY_FUNCTION__,__FILE__,__LINE__, &e);
	}
}

void Constraint::addExcludeElement(Column *column, Operator *oper, OperatorClass *op_class, bool use_sorting, bool asc_order, bool nulls_first)
{
	try
	{
		ExcludeElement elem;

		//Case the column is not allocated raises an error
		if(!column)
			throw Exception(Exception::getErrorMessage(ErrorCode::AsgNotAllocatedColumn)
							.arg(this->getName())
							.arg(this->getTypeName()),
							ErrorCode::AsgNotAllocatedColumn,__PRETTY_FUNCTION__,__FILE__,__LINE__);

		//Configures the element
		elem.setColumn(column);
		elem.setOperatorClass(op_class);
		elem.setOperator(oper);
		elem.setSortingEnabled(use_sorting);
		elem.setSortingAttribute(ExcludeElement::NullsFirst, nulls_first);
		elem.setSortingAttribute(ExcludeElement::AscOrder, asc_order);

		if(getExcludeElementIndex(elem) >= 0)
			throw Exception(ErrorCode::InsDuplicatedElement,__PRETTY_FUNCTION__,__FILE__,__LINE__);

		excl_elements.push_back(elem);
		setCodeInvalidated(true);
	}
	catch(Exception &e)
	{
		throw Exception(e.getErrorMessage(), e.getErrorCode(),__PRETTY_FUNCTION__,__FILE__,__LINE__, &e);
	}
}

void Constraint::removeExcludeElement(unsigned elem_idx)
{
	if(elem_idx >= excl_elements.size())
		throw Exception(ErrorCode::RefElementInvalidIndex,__PRETTY_FUNCTION__,__FILE__,__LINE__);

	excl_elements.erase(excl_elements.begin() + elem_idx);
	setCodeInvalidated(true);
}

void Constraint::removeExcludeElements()
{
	excl_elements.clear();
	setCodeInvalidated(true);
}

void Constraint::setColumnsNotNull(bool value)
{
	if(constr_type==ConstraintType::PrimaryKey)
	{
		for(auto &col : columns)
		{
			//if(!col->isAddedByRelationship())
			col->setNotNull(value);
		}
	}
}

ExcludeElement Constraint::getExcludeElement(unsigned elem_idx)
{
	if(elem_idx >= excl_elements.size())
		throw Exception(ErrorCode::RefElementInvalidIndex,__PRETTY_FUNCTION__,__FILE__,__LINE__);

	return excl_elements[elem_idx];
}

unsigned Constraint::getExcludeElementCount()
{
	return excl_elements.size();
}

void Constraint::setExcludeElementsAttribute(SchemaParser::CodeType def_type)
{
	QString str_elem;
	unsigned i, count;

	count=excl_elements.size();
	for(i=0; i < count; i++)
	{
		str_elem+=excl_elements[i].getSourceCode(def_type);
		if(i < (count-1) && def_type==SchemaParser::SqlCode) str_elem+=',';
	}

	attributes[Attributes::Elements]=str_elem;
}

void Constraint::setIndexType(IndexingType index_type)
{
	this->index_type=index_type;
}

IndexingType Constraint::getIndexType()
{
	return index_type;
}

QString Constraint::getSourceCode(SchemaParser::CodeType def_type)
{
	return getSourceCode(def_type, false);
}

void Constraint::setDeclInTableAttribute()
{
	if(!isDeclaredInTable() || (constr_type==ConstraintType::ForeignKey && !isAddedByLinking()))
		attributes[Attributes::DeclInTable]="";
	else if(!isReferRelationshipAddedColumn() || constr_type==ConstraintType::PrimaryKey)
		attributes[Attributes::DeclInTable]=Attributes::True;
}

void Constraint::configureSearchAttributes()
{
	TableObject::configureSearchAttributes();

	QStringList src_col_names, ref_col_names;

	for(auto &col : columns)
		src_col_names.append(col->getName());

	for(auto &col : ref_columns)
		ref_col_names.append(col->getSignature());

	search_attribs[Attributes::SrcColumns] = src_col_names.join(", ");
	search_attribs[Attributes::RefColumns] = ref_col_names.join(", ");
}

QString Constraint::getSourceCode(SchemaParser::CodeType def_type, bool inc_addedbyrel)
{
	QString code_def=getCachedCode(def_type, false);
	if(!inc_addedbyrel && !code_def.isEmpty()) return code_def;

	QString attrib;

	attributes[Attributes::PkConstr]="";
	attributes[Attributes::FkConstr]="";
	attributes[Attributes::CkConstr]="";
	attributes[Attributes::UqConstr]="";
	attributes[Attributes::ExConstr]="";

	switch(!constr_type)
	{
		case ConstraintType::Check:
			attrib=Attributes::CkConstr;
		break;
		case ConstraintType::PrimaryKey:
			attrib=Attributes::PkConstr;
		break;
		case ConstraintType::ForeignKey:
			attrib=Attributes::FkConstr;
		break;
		case ConstraintType::Unique:
			attrib=Attributes::UqConstr;
		break;
		default:
			attrib=Attributes::ExConstr;
		break;
	}
	attributes[attrib]=Attributes::True;

	attributes[Attributes::Type]=attrib;
	attributes[Attributes::UpdAction]=(~upd_action);
	attributes[Attributes::DelAction]=(~del_action);
	attributes[Attributes::Expression]=expression;

	if(constr_type!=ConstraintType::Check)
	{
		if(constr_type!=ConstraintType::Exclude)
			setColumnsAttribute(SourceCols, def_type, inc_addedbyrel);
		else
			setExcludeElementsAttribute(def_type);

		/* Only generates the definition of the foreign key referenced columns
		 if the number of columns of the source and referenced cols list are equal,
		 this means the constraint is configured correctly, otherwise don't generates
		 the attribute forcing the schema parser to return an error because the foreign key is
		 misconfigured. */
		if(constr_type==ConstraintType::ForeignKey && columns.size() == ref_columns.size())
			setColumnsAttribute(ReferencedCols, def_type, inc_addedbyrel);
	}

	attributes[Attributes::RefTable]=(ref_table ? ref_table->getName(true) : "");
	attributes[Attributes::Deferrable]=(deferrable ? Attributes::True : "");
	attributes[Attributes::NoInherit]=(no_inherit ? Attributes::True : "");
	attributes[Attributes::ComparisonType]=(~match_type);
	attributes[Attributes::DeferType]=(~deferral_type);
	attributes[Attributes::IndexType]=(~ index_type);

	if(getParentTable())
		attributes[Attributes::Table]=getParentTable()->getName(true);

	setDeclInTableAttribute();

	if(fill_factor!=0 && (constr_type==ConstraintType::PrimaryKey || constr_type==ConstraintType::Unique))
		attributes[Attributes::Factor]=QString("%1").arg(fill_factor);
	else
		attributes[Attributes::Factor]="";

	return BaseObject::__getSourceCode(def_type);
}

QString Constraint::getDropCode(bool cascade)
{
	setDeclInTableAttribute();
	return TableObject::getDropCode(cascade);
}

QString Constraint::getDataDictionary(const attribs_map &extra_attribs)
{
	try
	{
		attribs_map attribs;
		QStringList col_names;

		attribs.insert(extra_attribs.begin(), extra_attribs.end());
		attribs[Attributes::Name] = obj_name;
		attribs[Attributes::Type] = ~constr_type;
		attribs[Attributes::Comment] = comment;
		attribs[Attributes::RefTable] = ref_table ? ref_table->getSignature().remove('"') : "";
		attribs[Attributes::Expression] = expression;

		// Retrieving the columns that composes the constraint
		for(auto &col : columns)
			 col_names.push_back(col->getName());

		attribs[Attributes::Columns] = col_names.join(", ");

		schparser.ignoreEmptyAttributes(true);
		return schparser.getSourceCode(GlobalAttributes::getSchemaFilePath(GlobalAttributes::DataDictSchemaDir,
																																					 getSchemaName()), attribs);
	}
	catch(Exception &e)
	{
		throw Exception(e.getErrorMessage(), e.getErrorCode(), __PRETTY_FUNCTION__, __FILE__, __LINE__, &e);
	}
}

bool Constraint::isCodeDiffersFrom(BaseObject *object, const QStringList &ignored_attribs, const QStringList &ignored_tags)
{
	if(!object)
		throw Exception(ErrorCode::OprNotAllocatedObject,__PRETTY_FUNCTION__,__FILE__,__LINE__);
	else if(object->getObjectType()!=this->getObjectType())
		throw Exception(ErrorCode::OprObjectInvalidType,__PRETTY_FUNCTION__,__FILE__,__LINE__);

	try
	{
		return BaseObject::isCodeDiffersFrom(this->getSourceCode(SchemaParser::XmlCode, true),
											 object->getSourceCode(SchemaParser::XmlCode, true),
											 ignored_attribs, ignored_tags);
	}
	catch(Exception &e)
	{
		throw Exception(e.getErrorMessage(), e.getErrorCode(),__PRETTY_FUNCTION__,__FILE__,__LINE__, &e);
	}
}
