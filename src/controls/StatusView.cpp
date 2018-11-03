/*
 * Copyright 2002-2012, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Vlad Slepukhin
 *		Siarzhuk Zharski
 *
 * Copied from Haiku commit a609673ce8c942d91e14f24d1d8832951ab27964.
 * Modifications:
 * Copyright 2018 Kacper Kasper <kacperkasper@gmail.com>
 * Distributed under the terms of the MIT License.
 */


#include "StatusView.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <Catalog.h>
#include <ControlLook.h>
#include <DirMenu.h>
#include <MenuItem.h>
#include <Message.h>
#include <Messenger.h>
#include <PopUpMenu.h>
#include <ScrollView.h>
#include <StringView.h>
#include <Window.h>

#include "EditorWindow.h"


const float kHorzSpacing = 5.f;

using namespace BPrivate;


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "StatusView"


StatusView::StatusView(BScrollView* scrollView)
			:
			BView(BRect(), "statusview",
				B_FOLLOW_BOTTOM | B_FOLLOW_LEFT, B_WILL_DRAW),
			fScrollView(scrollView),
			fPreferredSize(0., 0.),
			fReadOnly(false),
			fNavigationPressed(false),
			fNavigationButtonWidth(B_H_SCROLL_BAR_HEIGHT)
{
	memset(fCellWidth, 0, sizeof(fCellWidth));
}


StatusView::~StatusView()
{
}


void
StatusView::AttachedToWindow()
{
	SetFont(be_plain_font);
	SetFontSize(10.);

	BMessage message(UPDATE_STATUS);
	message.AddInt32("line", 1);
	message.AddInt32("column", 1);
	message.AddString("type", "");
	SetStatus(&message);

	BScrollBar* scrollBar = fScrollView->ScrollBar(B_HORIZONTAL);
	MoveTo(0., scrollBar->Frame().top);

	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);

	ResizeToPreferred();
}


void
StatusView::GetPreferredSize(float* _width, float* _height)
{
	_ValidatePreferredSize();

	if (_width)
		*_width = fPreferredSize.width;

	if (_height)
		*_height = fPreferredSize.height;
}


void
StatusView::ResizeToPreferred()
{
	float width, height;
	GetPreferredSize(&width, &height);

	if (Bounds().Width() > width)
		width = Bounds().Width();

	BView::ResizeTo(width, height);
}


void
StatusView::Draw(BRect updateRect)
{
	if (fPreferredSize.width <= 0)
		return;

	rgb_color highColor = HighColor();
	BRect bounds(Bounds());
	bounds.bottom = fPreferredSize.height;
	bounds.right = fPreferredSize.width;
	uint32 flags = 0;
	if(!Window()->IsActive())
		flags |= BControlLook::B_DISABLED;
	be_control_look->DrawScrollBarBackground(this, bounds, bounds, ViewColor(),
		flags, B_HORIZONTAL);
	// DrawScrollBarBackground mutates the rect
	bounds.left = 0.0f;

	SetHighColor(tint_color(ViewColor(), B_DARKEN_2_TINT));
	StrokeLine(bounds.LeftTop(), bounds.RightTop());

	// Navigation button
	BRect navRect(bounds);
	navRect.left--;
	navRect.right = fNavigationButtonWidth + 1;
	StrokeLine(navRect.RightTop(), navRect.RightBottom());
	_DrawNavigationButton(navRect);
	bounds.left = navRect.right + 1;

	// BControlLook mutates color
	SetHighColor(tint_color(ViewColor(), B_DARKEN_2_TINT));

	float x = bounds.left;
	for (size_t i = 0; i < kStatusCellCount - 1; i++) {
		if (fCellText[i].IsEmpty())
			continue;
		x += fCellWidth[i];
		if (fCellText[i + 1].IsEmpty() == false)
			StrokeLine(BPoint(x, bounds.top + 3), BPoint(x, bounds.bottom - 3));
	}

	SetLowColor(ViewColor());
	SetHighColor(highColor);

	font_height fontHeight;
	GetFontHeight(&fontHeight);

	x = bounds.left;
	float y = (bounds.bottom + bounds.top
		+ ceilf(fontHeight.ascent) - ceilf(fontHeight.descent)) / 2;

	for (size_t i = 0; i < kStatusCellCount; i++) {
		if (fCellText[i].Length() == 0)
			continue;
		DrawString(fCellText[i], BPoint(x + kHorzSpacing, y));
		x += fCellWidth[i];
	}
}


void
StatusView::MouseDown(BPoint where)
{
	if (where.x < B_H_SCROLL_BAR_HEIGHT && _HasRef()) {
		fNavigationPressed = true;
		Invalidate();
		_ShowDirMenu();
		return;
	}

	if(where.x < fNavigationButtonWidth + fCellWidth[kPositionCell]) {
		BMessenger msgr(Window());
		msgr.SendMessage(MAINMENU_SEARCH_GOTOLINE);
	}

	if (!fReadOnly)
		return;

	float left = fNavigationButtonWidth + fCellWidth[kPositionCell] + fCellWidth[kTypeCell];
	if (where.x < left)
		return;

	int32 clicks = 0;
	BMessage* message = Window()->CurrentMessage();
	if (message != NULL
		&& message->FindInt32("clicks", &clicks) == B_OK && clicks > 1)
			return;
}


void
StatusView::WindowActivated(bool active)
{
	// Workaround: doesn't redraw automatically
	Invalidate();
}


void
StatusView::SetStatus(BMessage* message)
{
	int32 line = 0, column = 0;
	if (message->FindInt32("line", &line) == B_OK
		&& message->FindInt32("column", &column) == B_OK)
	{
		fCellText[kPositionCell].SetToFormat("%d, %d", line, column);
	}

	if (message->FindString("type", &fType) == B_OK) {
		fCellText[kTypeCell] = fType;
	}

	fReadOnly = false;
	if (message->FindBool("readOnly", &fReadOnly) == B_OK && fReadOnly) {
		fCellText[kFileStateCell] = B_TRANSLATE("Read-only");
	} else
		fCellText[kFileStateCell].Truncate(0);

	_ValidatePreferredSize();
	Invalidate();
}


void
StatusView::SetRef(const entry_ref& ref)
{
	fRef = ref;
}


bool
StatusView::_HasRef()
{
	return fRef != entry_ref();
}


void
StatusView::_ValidatePreferredSize()
{
	// width
	fPreferredSize.width = fNavigationButtonWidth;
	for (size_t i = 0; i < kStatusCellCount; i++) {
		if (fCellText[i].Length() == 0) {
			fCellWidth[i] = 0;
			continue;
		}
		float width = ceilf(StringWidth(fCellText[i]));
		if (width > 0)
			width += kHorzSpacing * 2;
		if (width > fCellWidth[i] || i != kPositionCell)
			fCellWidth[i] = width;
		fPreferredSize.width += fCellWidth[i];
	}

	// height
	font_height fontHeight;
	GetFontHeight(&fontHeight);

	fPreferredSize.height = ceilf(fontHeight.ascent + fontHeight.descent
		+ fontHeight.leading);

	if (fPreferredSize.height < B_H_SCROLL_BAR_HEIGHT)
		fPreferredSize.height = B_H_SCROLL_BAR_HEIGHT;

	ResizeBy(fPreferredSize.width, 0);
	BScrollBar* scrollBar = fScrollView->ScrollBar(B_HORIZONTAL);
	float diff = scrollBar->Frame().left - fPreferredSize.width;
	if(fabs(diff) > 0.5) {
		scrollBar->ResizeBy(diff, 0);
		scrollBar->MoveBy(-diff, 0);
	}
}


void
StatusView::_DrawNavigationButton(BRect rect)
{
	rect.InsetBy(1.0f, 1.0f);
	rgb_color baseColor = tint_color(ui_color(B_PANEL_BACKGROUND_COLOR),
		B_LIGHTEN_1_TINT);
	uint32 flags = 0;
	if(fNavigationPressed)
		flags |= BControlLook::B_ACTIVATED;
	if(Window()->IsActive() == false || _HasRef() == false)
		flags |= BControlLook::B_DISABLED;
	be_control_look->DrawButtonBackground(this, rect, rect, baseColor, flags,
		BControlLook::B_ALL_BORDERS, B_HORIZONTAL);
	rect.InsetBy(0.0f, -1.0f);
	be_control_look->DrawArrowShape(this, rect, rect, baseColor,
		BControlLook::B_DOWN_ARROW, flags, B_DARKEN_MAX_TINT);
}


void
StatusView::_ShowDirMenu()
{
	BEntry entry;
	status_t status = entry.SetTo(&fRef);

	if (status != B_OK || !entry.Exists())
		return;

	BPrivate::BDirMenu* menu = new BDirMenu(NULL,
		BMessenger(Window()), B_REFS_RECEIVED);

	BMenuItem* openTerminal = new BMenuItem(B_TRANSLATE("Open Terminal"),
		new BMessage((uint32) OPEN_TERMINAL), 'T', B_OPTION_KEY);
		// Actual shortcut is added in EditorWindow
	openTerminal->SetTarget(Window());
	menu->AddItem(openTerminal);
	menu->AddSeparatorItem();
	menu->Populate(&entry, Window(), false, false, true, false, true);

	BPoint point = Parent()->Bounds().LeftBottom();
	point.y += 3 + B_H_SCROLL_BAR_HEIGHT;
	ConvertToScreen(&point);
	BRect clickToOpenRect(Parent()->Bounds());
	ConvertToScreen(&clickToOpenRect);
	menu->Go(point, true, true, clickToOpenRect);
	fNavigationPressed = false;
	delete menu;
}
