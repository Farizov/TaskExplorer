#include "stdafx.h"
#include "PanelView.h"

bool CPanelView::m_SimpleFormat = false;
int CPanelView::m_MaxCellWidth = 0;

CPanelView::CPanelView(QWidget *parent)
	:QWidget(parent)
{
	//m_CopyAll = false;

	m_pMenu = new QMenu();
}

CPanelView::~CPanelView()
{
}

void CPanelView::AddPanelItemsToMenu(bool bAddSeparator)
{
	if(bAddSeparator)
		m_pMenu->addSeparator();
	m_pCopyCell = m_pMenu->addAction(tr("Copy Cell"), this, SLOT(OnCopyCell()));
	m_pCopyRow = m_pMenu->addAction(tr("Copy Row"), this, SLOT(OnCopyRow()));
	m_pCopyPanel = m_pMenu->addAction(tr("Copy Panel"), this, SLOT(OnCopyPanel()));
}

void CPanelView::OnMenu(const QPoint& Point)
{
	QModelIndex Index = GetView()->currentIndex();
	
	m_pCopyCell->setEnabled(Index.isValid());
	m_pCopyRow->setEnabled(Index.isValid());

	m_pMenu->popup(QCursor::pos());	
}

void CPanelView::OnCopyCell()
{
	QAbstractItemModel* pModel = GetModel();
	QTreeView * pView = GetView();

	QModelIndex Index = pView->currentIndex();
	QModelIndex ModelIndex = MapToSource(Index);
	int Column = ModelIndex.column();

	QList<QStringList> Rows;
	foreach(const QModelIndex& Index, pView->selectionModel()->selectedIndexes())
	{
		if (Index.column() != Column)
			continue;
		QModelIndex CurIndex = pModel->index(Index.row(), Column, Index.parent());
		QString Cell = pModel->data(CurIndex, Qt::DisplayRole).toString();
		Rows.append(QStringList() << Cell);
	}
	FormatAndCopy(Rows, false);
}

void CPanelView::OnCopyRow()
{
	QAbstractItemModel* pModel = GetModel();
	QTreeView * pView = GetView();

	int Column = 0; // find first not hidden column
	for (int i = 0; i < pModel->columnCount(); i++)
	{
		if (!pView->isColumnHidden(i) || m_ForcedColumns.contains(i))
		{
			Column = i;
			break;
		}
	}

	QList<QStringList> Rows;
	foreach(const QModelIndex& Index, pView->selectionModel()->selectedIndexes())
	{
		if (Index.column() != Column)
			continue;

		QModelIndex ModelIndex = MapToSource(Index);
		Rows.append(CopyRow(ModelIndex));
	}
	FormatAndCopy(Rows);
}

QStringList CPanelView::CopyHeader()
{
	QAbstractItemModel* pModel = GetModel();
	QTreeView * pView = GetView();

	QStringList Headder;
	for (int i = 0; i < pModel->columnCount(); i++)
	{
		if (/*!m_CopyAll &&*/ pView->isColumnHidden(i) && !m_ForcedColumns.contains(i))
			continue;
		QString Cell = pModel->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString();
		if (!m_SimpleFormat)
			Cell = "|" + Cell + "|";
		Headder.append(Cell);
	}
	return Headder;
}

QStringList CPanelView::CopyRow(const QModelIndex& ModelIndex, int Level)
{
	QAbstractItemModel* pModel = GetModel();
	QTreeView * pView = GetView();

	QStringList Cells;
	for (int i = 0; i < pModel->columnCount(); i++)
	{
		if (/*!m_CopyAll &&*/ pView->isColumnHidden(i) && !m_ForcedColumns.contains(i))
			continue;
		QModelIndex CellIndex = pModel->index(ModelIndex.row(), i, ModelIndex.parent());
		QString Cell = pModel->data(CellIndex, Qt::DisplayRole).toString();
		if (Level && i == 0)
			Cell.prepend(QString(Level, '_') + " ");
		Cells.append(Cell);
	}
	return Cells;
}

void CPanelView::RecursiveCopyPanel(const QModelIndex& ModelIndex, QList<QStringList>& Rows, int Level)
{
	QAbstractItemModel* pModel = GetModel();

	Rows.append(CopyRow(ModelIndex, Level));

	for (int i = 0; i < pModel->rowCount(ModelIndex); i++)
	{
		QModelIndex SubIndex = pModel->index(i, 0, ModelIndex);
		RecursiveCopyPanel(SubIndex, Rows, Level + 1);
	}
}

void CPanelView::OnCopyPanel()
{
	QAbstractItemModel* pModel = GetModel();

	QList<QStringList> Rows;
	for (int i = 0; i < pModel->rowCount(); ++i)
	{
		QModelIndex ModelIndex = pModel->index(i, 0);
		RecursiveCopyPanel(ModelIndex, Rows);
	}
	FormatAndCopy(Rows);
}

void CPanelView::FormatAndCopy(QList<QStringList> Rows, bool Headder)
{
	int RowCount = Rows.length();

	if (Headder)
	{
		Rows.prepend(QStringList());
		Rows.prepend(CopyHeader());
		Rows.prepend(QStringList());
	}

	QStringList TextRows;
	if (m_SimpleFormat || !Headder)
	{
		foreach(const QStringList& Row, Rows)
			TextRows.append(Row.join("\t"));
	}
	else if(Rows.size() > (Headder ? 3 : 0))
	{
		int Columns = Rows[Headder ? 3 : 0].count();
		QVector<int> ColumnWidths(Columns, 0);

		foreach(const QStringList& Row, Rows)
		{
			for (int i = 0; i < Min(Row.count(), Columns); i++)
			{
				int CellWidth = Row[i].length();
				if (ColumnWidths[i] < CellWidth)
					ColumnWidths[i] = CellWidth;
			}
		}

		foreach(const QStringList& Row, Rows)
		{
			if (m_MaxCellWidth != 0 && RowCount > 1)
			{				
				for (int Pos = 0;;Pos += m_MaxCellWidth)
				{
					bool More = false;

					QString RowText;
					for (int i = 0; i < Min(Row.count(), Columns); i++) 
					{
						if (Row[i].length() > Pos)
							RowText.append(Row[i].mid(Pos, m_MaxCellWidth).leftJustified(Min(m_MaxCellWidth, ColumnWidths[i]) + 3));
						else
							RowText.append(QString(Min(m_MaxCellWidth, ColumnWidths[i]) + 3, ' '));

						if (Row[i].length() > Pos + m_MaxCellWidth)
							More = true;
					}
					TextRows.append(RowText);

					if (!More)
						break;
				}
			}
			else
			{
				QString RowText;
				for (int i = 0; i < Min(Row.count(), Columns); i++)
					RowText.append(Row[i].leftJustified(ColumnWidths[i] + 3));
				TextRows.append(RowText);
			}
		}
	}
	QApplication::clipboard()->setText(TextRows.join("\n"));
}