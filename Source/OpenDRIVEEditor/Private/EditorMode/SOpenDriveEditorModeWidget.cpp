#include "Public/EditorMode/SOpenDriveEditorModeWidget.h"
#include "Public/OpenDriveEditor.h"
#include "Public/EditorMode/OpenDriveEditorMode.h"
#include "Public/SplineGenerator.h"
#include "SignalTypeMapping.h"
#include "SignalAssemblyMapping.h"

void SOpenDRIVEEditorModeWidget::Construct(const FArguments& InArgs)
{
	_fontInfoPtr = MakeShareable(new FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Light.ttf"), 12));

	ChildSlot
	[
		SNew(SVerticalBox)
		// Tab selector bar
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(20.f, 20.f, 20.f, 0.f)
		.HAlign(HAlign_Center)
		[
			ConstructTabBar(InArgs)
		]
		// Tab content area
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(20.f, 10.f, 20.f, 0.f)
		[
			SNew(SBorder)
			[
				SAssignNew(_tabSwitcher, SWidgetSwitcher)
				.WidgetIndex_Lambda([this]() { return _activeTabIndex; })
				+ SWidgetSwitcher::Slot() [ ConstructRoadTabContent(InArgs) ]
				+ SWidgetSwitcher::Slot() [ ConstructSplineTabContent(InArgs) ]
				+ SWidgetSwitcher::Slot() [ ConstructSignalTabContent(InArgs) ]
			]
		]
		// Lane Info (always visible)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(20.f, 20.f, 20.f, 0.f)
		[
			ConstructLaneInfoBox(InArgs)
		]
	];
}

TSharedRef<SWidget> SOpenDRIVEEditorModeWidget::ConstructTabBar(const FArguments& InArgs)
{
	return
		SNew(SSegmentedControl<int32>)
		.UniformPadding(FMargin(12.f, 4.f))
		.Value(0)
		.OnValueChanged_Lambda([this](int32 NewIndex)
		{
			_activeTabIndex = NewIndex;
		})
		+ SSegmentedControl<int32>::Slot(0)
			.Text(FText::FromString("Road"))
		+ SSegmentedControl<int32>::Slot(1)
			.Text(FText::FromString("Spline"))
		+ SSegmentedControl<int32>::Slot(2)
			.Text(FText::FromString("Signal"));
}

TSharedRef<SBorder> SOpenDRIVEEditorModeWidget::ConstructLaneInfoBox(const FArguments& InArgs)
{
	_roadIdTextPtr = SNew(STextBlock)
		.Text(FText::FromString(TEXT("RoadId : ")))
		.Font(*_fontInfoPtr)
		.ToolTipText(FText::FromString(TEXT("The selected road's Id.")));

	_junctionIdTextPtr = SNew(STextBlock)
		.Text(FText::FromString(TEXT("JunctionId : ")))
		.Font(*_fontInfoPtr)
		.ToolTipText(FText::FromString(TEXT("The selected road's junction's Id.")));

	_laneTypeTextPtr = SNew(STextBlock)
		.Text(FText::FromString(TEXT("Lane type : ")))
		.Font(*_fontInfoPtr)
		.ToolTipText(FText::FromString(TEXT("The selected lane's type.")));

	_laneIdTextPtr = SNew(STextBlock)
		.Text(FText::FromString(TEXT("LaneId : ")))
		.Font(*_fontInfoPtr)
		.ToolTipText(FText::FromString(TEXT("The selected lane's Id.")));

	_successorIdTextPtr = SNew(STextBlock)
		.Text(FText::FromString(TEXT("Successor Id : ")))
		.Font(*_fontInfoPtr)
		.ToolTipText(FText::FromString(TEXT("The road's successorId.")));

	_predecessorIdTextPtr = SNew(STextBlock)
		.Text(FText::FromString(TEXT("Predecessor Id : ")))
		.Font(*_fontInfoPtr)
		.ToolTipText(FText::FromString(TEXT("The road's predecessorId.")));

	TSharedRef<SBorder> border =
		SNew(SBorder)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(10.f, 10.f, 10.f, 0.f)
			[
				SNew(SBorder)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(10.f, 10.f, 0.f, 0.f)
					[
						_roadIdTextPtr.ToSharedRef()
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(10.f, 5.f, 0.f, 0.f)
					[
						_successorIdTextPtr.ToSharedRef()
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(10.f, 5.f, 0.f, 0.f)
					[
						_predecessorIdTextPtr.ToSharedRef()
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(10.f, 5.f, 0.f, 10.f)
					[
						_junctionIdTextPtr.ToSharedRef()
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(10.f, 20.f, 10.f, 10.f)
			[
				SNew(SBorder)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(10.f, 10.f, 0.f, 0.f)
					[
						_laneIdTextPtr.ToSharedRef()
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(10.f, 5.f, 0.f, 10.f)
					[
						_laneTypeTextPtr.ToSharedRef()
					]
				]
			]

		];
	return border;
}

TSharedRef<SWidget> SOpenDRIVEEditorModeWidget::ConstructRoadTabContent(const FArguments& InArgs)
{
	// Reset Button
	TSharedPtr<SButton> resetButton = SNew(SButton).Text(FText::FromString("Reset"))
		.OnClicked(this, &SOpenDRIVEEditorModeWidget::Reset).IsEnabled(this, &SOpenDRIVEEditorModeWidget::IsLoaded)
		.ToolTipText(FText::FromString(TEXT("Resets currently drawn roads.")));
	StaticCast<STextBlock&>(resetButton.ToSharedRef().Get().GetContent().Get()).SetJustification(ETextJustify::Center);

	// Generate Button
	TSharedPtr<SButton> generateButton = SNew(SButton).Text(FText::FromString("Generate"))
		.OnClicked(this, &SOpenDRIVEEditorModeWidget::Generate).IsEnabled(this, &SOpenDRIVEEditorModeWidget::CheckIfInEditorMode)
		.ToolTipText(FText::FromString(TEXT("Draws roads (will reset currently drawn roads).")));
	StaticCast<STextBlock&>(generateButton.ToSharedRef().Get().GetContent().Get()).SetJustification(ETextJustify::Center);

	// Sliders
	_offsetTextPtr = SNew(STextBlock).Justification(ETextJustify::Center)
		.Text(FText::FromString("ZOffset : " + FString::FormatAsNumber(GetEdMode()->GetRoadOffset())))
		.Font(*_fontInfoPtr)
		.ToolTipText(FText::FromString(TEXT("Roads' ZOffset : by defaults, the road's network will be drawn at Z=20.\nSo if you already have static meshes to represent your roads, higher that value to avoid texture flickering.")));

	_stepTextPtr = SNew(STextBlock).Justification(ETextJustify::Center)
		.Text(FText::FromString("Step : " + FString::FormatAsNumber(GetEdMode()->GetStep())))
		.Font(*_fontInfoPtr)
		.ToolTipText(FText::FromString(TEXT("Lower this value for a more precise draw (and less performances !).")));

	TSharedRef<SSlider> OffsetSlider = SNew(SSlider).MinValue(0.f).MaxValue(80.f)
		.Value(GetEdMode()->GetRoadOffset())
		.OnValueChanged(this, &SOpenDRIVEEditorModeWidget::OnOffsetValueChanged);

	TSharedRef<SSlider> StepSlider = SNew(SSlider).MinValue(1.f).MaxValue(10.f)
		.Value(GetEdMode()->GetStep())
		.OnValueChanged(this, &SOpenDRIVEEditorModeWidget::OnStepValueChanged);

	_showArrowsCheckBox = SNew(SCheckBox)
		.IsEnabled(this, &SOpenDRIVEEditorModeWidget::CheckIfInEditorMode)
		.OnCheckStateChanged(this, &SOpenDRIVEEditorModeWidget::OnCheckStateChanged);

	return SNew(SVerticalBox)
		// Buttons: Reset | Generate
		+ SVerticalBox::Slot().AutoHeight().Padding(5.f, 5.f, 5.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.5f).Padding(0, 0, 5, 0)
			[
				resetButton.ToSharedRef()
			]
			+ SHorizontalBox::Slot().FillWidth(0.5f).Padding(5, 0, 0, 0)
			[
				generateButton.ToSharedRef()
			]
		]
		// Show Arrows (label + checkbox on same line)
		+ SVerticalBox::Slot().AutoHeight().Padding(5.f, 10.f, 5.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(5, 0, 10, 0)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Show arrows")))
				.Font(*_fontInfoPtr)
				.ToolTipText(FText::FromString(TEXT("Tick the checkbox to see the roads' directions.")))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				_showArrowsCheckBox.ToSharedRef()
			]
		]
		// ZOffset slider
		+ SVerticalBox::Slot().AutoHeight().Padding(5.f, 10.f, 0.f, 0.f)
		[
			_offsetTextPtr.ToSharedRef()
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(5.f, 5.f, 5.f, 0.f)
		[
			OffsetSlider
		]
		// Step slider
		+ SVerticalBox::Slot().AutoHeight().Padding(5.f, 5.f, 0.f, 0.f)
		[
			_stepTextPtr.ToSharedRef()
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(5.f, 5.f, 5.f, 10.f)
		[
			StepSlider
		];
}

TSharedRef<SWidget> SOpenDRIVEEditorModeWidget::ConstructSplineTabContent(const FArguments& InArgs)
{
	// Gen Splines Button
	TSharedPtr<SButton> generateSplinesButton = SNew(SButton).Text(FText::FromString("Gen Splines"))
		.OnClicked(this, &SOpenDRIVEEditorModeWidget::GenerateLaneSplines).IsEnabled(this, &SOpenDRIVEEditorModeWidget::CheckIfInEditorMode)
		.ToolTipText(FText::FromString(TEXT("Generates persistent spline actors for all lanes. Previously generated splines are kept.")));
	StaticCast<STextBlock&>(generateSplinesButton.ToSharedRef().Get().GetContent().Get()).SetJustification(ETextJustify::Center);

	// Clear Splines Button
	TSharedPtr<SButton> clearSplinesButton = SNew(SButton).Text(FText::FromString("Clear Splines"))
		.OnClicked(this, &SOpenDRIVEEditorModeWidget::ClearGeneratedSplines).IsEnabled(this, &SOpenDRIVEEditorModeWidget::CheckIfInEditorMode)
		.ToolTipText(FText::FromString(TEXT("Clears all previously generated spline actors.")));
	StaticCast<STextBlock&>(clearSplinesButton.ToSharedRef().Get().GetContent().Get()).SetJustification(ETextJustify::Center);

	// Select All / Deselect All buttons
	TSharedPtr<SButton> selectAllButton = SNew(SButton).Text(FText::FromString("All"))
		.OnClicked_Lambda([this]() { SetAllLaneTypeCheckBoxes(true); return FReply::Handled(); })
		.ToolTipText(FText::FromString(TEXT("Check all lane type filters.")));
	StaticCast<STextBlock&>(selectAllButton.ToSharedRef().Get().GetContent().Get()).SetJustification(ETextJustify::Center);

	TSharedPtr<SButton> deselectAllButton = SNew(SButton).Text(FText::FromString("None"))
		.OnClicked_Lambda([this]() { SetAllLaneTypeCheckBoxes(false); return FReply::Handled(); })
		.ToolTipText(FText::FromString(TEXT("Uncheck all lane type filters.")));
	StaticCast<STextBlock&>(deselectAllButton.ToSharedRef().Get().GetContent().Get()).SetJustification(ETextJustify::Center);

	// Spline generation mode combo options
	_splineGenModeOptions.Add(MakeShareable(new FString("Center")));
	_splineGenModeOptions.Add(MakeShareable(new FString("Inside")));
	_splineGenModeOptions.Add(MakeShareable(new FString("Outside")));

	_splineGenModeComboBox = SNew(SComboBox<TSharedPtr<FString>>)
		.OptionsSource(&_splineGenModeOptions)
		.OnGenerateWidget(this, &SOpenDRIVEEditorModeWidget::MakeSplineResampleModeWidget)
		.OnSelectionChanged(this, &SOpenDRIVEEditorModeWidget::OnSplineResampleModeChanged)
		[
			SNew(STextBlock).Text_Lambda([this]()
			{
				if (_splineGenModeComboBox.IsValid() && _splineGenModeComboBox->GetSelectedItem().IsValid())
				{
					return FText::FromString(*_splineGenModeComboBox->GetSelectedItem());
				}
				return FText::FromString("Center");
			})
			.Font(*_fontInfoPtr)
		];

	// Set initial selection based on mode
	AOpenDriveLaneSpline::EOpenDriveLaneSplineMode CurrentMode = GetEdMode()->GetSplineGenerationMode();
	if (_splineGenModeOptions.IsValidIndex((int)CurrentMode))
	{
		_splineGenModeComboBox->SetSelectedItem(_splineGenModeOptions[(int)CurrentMode]);
	}

	// Lane position filter combo options
	_lanePositionFilterOptions.Add(MakeShareable(new FString("All")));
	_lanePositionFilterOptions.Add(MakeShareable(new FString("Outermost Only")));
	_lanePositionFilterOptions.Add(MakeShareable(new FString("Outermost Driving Only")));
	_lanePositionFilterOptions.Add(MakeShareable(new FString("Innermost Only")));
	_lanePositionFilterOptions.Add(MakeShareable(new FString("Innermost Driving Only")));
	_lanePositionFilterOptions.Add(MakeShareable(new FString("Specific Index")));

	_lanePositionFilterComboBox = SNew(SComboBox<TSharedPtr<FString>>)
		.OptionsSource(&_lanePositionFilterOptions)
		.OnGenerateWidget(this, &SOpenDRIVEEditorModeWidget::MakeLanePositionFilterWidget)
		.OnSelectionChanged(this, &SOpenDRIVEEditorModeWidget::OnLanePositionFilterChanged)
		[
			SNew(STextBlock).Text_Lambda([this]()
			{
				if (_lanePositionFilterComboBox.IsValid() && _lanePositionFilterComboBox->GetSelectedItem().IsValid())
				{
					return FText::FromString(*_lanePositionFilterComboBox->GetSelectedItem());
				}
				return FText::FromString("All");
			})
			.Font(*_fontInfoPtr)
		];
	_lanePositionFilterComboBox->SetSelectedItem(_lanePositionFilterOptions[0]);

	return SNew(SVerticalBox)
		// Buttons: Gen Splines | Clear Splines
		+ SVerticalBox::Slot().AutoHeight().Padding(5.f, 5.f, 5.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.5f).Padding(0, 0, 5, 0)
			[
				generateSplinesButton.ToSharedRef()
			]
			+ SHorizontalBox::Slot().FillWidth(0.5f).Padding(5, 0, 0, 0)
			[
				clearSplinesButton.ToSharedRef()
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f).HAlign(HAlign_Center) [ SNew(SSeparator) ]
		// Spline Generation Reference + Lane Position Filter (side by side)
		+ SVerticalBox::Slot().AutoHeight().Padding(5.f, 5.f, 5.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.5f).Padding(0, 0, 5, 0)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(FText::FromString("Reference")).Font(*_fontInfoPtr).Justification(ETextJustify::Center)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)
				[
					_splineGenModeComboBox.ToSharedRef()
				]
			]
			+ SHorizontalBox::Slot().FillWidth(0.5f).Padding(5, 0, 0, 0)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(FText::FromString("Position")).Font(*_fontInfoPtr).Justification(ETextJustify::Center)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
					[
						_lanePositionFilterComboBox.ToSharedRef()
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(5, 0, 0, 0).VAlign(VAlign_Center)
					[
						SAssignNew(_specificLaneIndexSpinBox, SSpinBox<int32>)
							.MinValue(1)
							.MaxValue(10)
							.Value(1)
							.OnValueCommitted(this, &SOpenDRIVEEditorModeWidget::OnSpecificLaneIndexChanged)
							.Visibility_Lambda([this]() -> EVisibility
							{
								if (_lanePositionFilterComboBox.IsValid() && _lanePositionFilterComboBox->GetSelectedItem().IsValid())
								{
									return (*_lanePositionFilterComboBox->GetSelectedItem() == "Specific Index")
										? EVisibility::Visible : EVisibility::Collapsed;
								}
								return EVisibility::Collapsed;
							})
					]
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f).HAlign(HAlign_Center) [ SNew(SSeparator) ]
		// General Filters
		+ SVerticalBox::Slot().AutoHeight().Padding(5.f, 5.f, 5.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(5) [ SNew(SCheckBox).IsChecked(ECheckBoxState::Checked).OnCheckStateChanged(this, &SOpenDRIVEEditorModeWidget::OnGenerateRoadsCheckStateChanged) .Content()[ SNew(STextBlock).Text(FText::FromString("Roads")) ] ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(5) [ SNew(SCheckBox).IsChecked(ECheckBoxState::Checked).OnCheckStateChanged(this, &SOpenDRIVEEditorModeWidget::OnGenerateJunctionsCheckStateChanged) .Content()[ SNew(STextBlock).Text(FText::FromString("Junctions")) ] ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(5)
			[
				SAssignNew(_leftLanesCheckBox, SCheckBox)
					.IsChecked(ECheckBoxState::Checked)
					.OnCheckStateChanged(this, &SOpenDRIVEEditorModeWidget::OnLeftLanesCheckStateChanged)
					.Content()[ SNew(STextBlock).Text(FText::FromString("Left")) ]
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(5)
			[
				SAssignNew(_rightLanesCheckBox, SCheckBox)
					.IsChecked(ECheckBoxState::Checked)
					.OnCheckStateChanged(this, &SOpenDRIVEEditorModeWidget::OnRightLanesCheckStateChanged)
					.Content()[ SNew(STextBlock).Text(FText::FromString("Right")) ]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f).HAlign(HAlign_Center) [ SNew(SSeparator) ]
		// Lane Types header + All/None buttons
		+ SVerticalBox::Slot().AutoHeight().Padding(5.f, 5.f, 5.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(5, 0, 0, 0)
			[
				SNew(STextBlock).Text(FText::FromString("Lane Types")).Font(*_fontInfoPtr)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 2, 0)
			[
				selectAllButton.ToSharedRef()
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2, 0, 5, 0)
			[
				deselectAllButton.ToSharedRef()
			]
		]
		// Lane Types - 3 column grid
		+ SVerticalBox::Slot().AutoHeight().Padding(5.f, 2.f, 5.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f / 3.f).Padding(5, 2) [ SAssignNew(_drivingCheckBox, SCheckBox).IsChecked(ECheckBoxState::Checked).OnCheckStateChanged(this, &SOpenDRIVEEditorModeWidget::OnDrivingLaneCheckStateChanged) .Content()[ SNew(STextBlock).Text(FText::FromString("Driving")) ] ]
			+ SHorizontalBox::Slot().FillWidth(1.f / 3.f).Padding(5, 2) [ SAssignNew(_sidewalkCheckBox, SCheckBox).IsChecked(ECheckBoxState::Checked).OnCheckStateChanged(this, &SOpenDRIVEEditorModeWidget::OnSidewalkLaneCheckStateChanged) .Content()[ SNew(STextBlock).Text(FText::FromString("Sidewalk")) ] ]
			+ SHorizontalBox::Slot().FillWidth(1.f / 3.f).Padding(5, 2) [ SAssignNew(_bikingCheckBox, SCheckBox).IsChecked(ECheckBoxState::Checked).OnCheckStateChanged(this, &SOpenDRIVEEditorModeWidget::OnBikingLaneCheckStateChanged) .Content()[ SNew(STextBlock).Text(FText::FromString("Biking")) ] ]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(5.f, 0.f, 5.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f / 3.f).Padding(5, 2) [ SAssignNew(_parkingCheckBox, SCheckBox).IsChecked(ECheckBoxState::Checked).OnCheckStateChanged(this, &SOpenDRIVEEditorModeWidget::OnParkingLaneCheckStateChanged) .Content()[ SNew(STextBlock).Text(FText::FromString("Parking")) ] ]
			+ SHorizontalBox::Slot().FillWidth(1.f / 3.f).Padding(5, 2) [ SAssignNew(_shoulderCheckBox, SCheckBox).IsChecked(ECheckBoxState::Checked).OnCheckStateChanged(this, &SOpenDRIVEEditorModeWidget::OnShoulderLaneCheckStateChanged) .Content()[ SNew(STextBlock).Text(FText::FromString("Shoulder")) ] ]
			+ SHorizontalBox::Slot().FillWidth(1.f / 3.f).Padding(5, 2) [ SAssignNew(_restrictedCheckBox, SCheckBox).IsChecked(ECheckBoxState::Checked).OnCheckStateChanged(this, &SOpenDRIVEEditorModeWidget::OnRestrictedLaneCheckStateChanged) .Content()[ SNew(STextBlock).Text(FText::FromString("Restricted")) ] ]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(5.f, 0.f, 5.f, 10.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f / 3.f).Padding(5, 2) [ SAssignNew(_medianCheckBox, SCheckBox).IsChecked(ECheckBoxState::Checked).OnCheckStateChanged(this, &SOpenDRIVEEditorModeWidget::OnMedianLaneCheckStateChanged) .Content()[ SNew(STextBlock).Text(FText::FromString("Median")) ] ]
			+ SHorizontalBox::Slot().FillWidth(1.f / 3.f).Padding(5, 2) [ SAssignNew(_otherCheckBox, SCheckBox).IsChecked(ECheckBoxState::Checked).OnCheckStateChanged(this, &SOpenDRIVEEditorModeWidget::OnOtherLaneCheckStateChanged) .Content()[ SNew(STextBlock).Text(FText::FromString("Other")) ] ]
			+ SHorizontalBox::Slot().FillWidth(1.f / 3.f).Padding(5, 2) [ SAssignNew(_referenceCheckBox, SCheckBox).IsChecked(ECheckBoxState::Checked).OnCheckStateChanged(this, &SOpenDRIVEEditorModeWidget::OnReferenceLaneCheckStateChanged) .Content()[ SNew(STextBlock).Text(FText::FromString("Reference")) ] ]
		];
}

TSharedRef<SWidget> SOpenDRIVEEditorModeWidget::ConstructSignalTabContent(const FArguments& InArgs)
{
	// Gen Signals Button
	TSharedPtr<SButton> generateSignalsButton = SNew(SButton).Text(FText::FromString("Gen Signals"))
		.OnClicked(this, &SOpenDRIVEEditorModeWidget::GenerateSignals).IsEnabled(this, &SOpenDRIVEEditorModeWidget::CheckIfInEditorMode)
		.ToolTipText(FText::FromString(TEXT("Generates signal actors from OpenDRIVE data.")));
	StaticCast<STextBlock&>(generateSignalsButton.ToSharedRef().Get().GetContent().Get()).SetJustification(ETextJustify::Center);

	return SNew(SVerticalBox)
		// Button: Gen Signals
		+ SVerticalBox::Slot().AutoHeight().Padding(5.f, 5.f, 5.f, 0.f)
		[
			generateSignalsButton.ToSharedRef()
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f).HAlign(HAlign_Center) [ SNew(SSeparator) ]
		// Generate Signals + Flip Orientation checkboxes
		+ SVerticalBox::Slot().AutoHeight().Padding(5.f, 5.f, 0.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(5)
			[
				SAssignNew(_generateSignalsCheckBox, SCheckBox)
					.IsChecked(ECheckBoxState::Checked)
					.OnCheckStateChanged(this, &SOpenDRIVEEditorModeWidget::OnGenerateSignalsCheckStateChanged)
					.Content()
					[
						SNew(STextBlock).Text(FText::FromString("Generate Signals"))
					]
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(15, 0, 5, 0)
			[
				SAssignNew(_flipSignalOrientationCheckBox, SCheckBox)
					.IsChecked(ECheckBoxState::Unchecked)
					.OnCheckStateChanged(this, &SOpenDRIVEEditorModeWidget::OnFlipSignalOrientationCheckStateChanged)
					.Content()
					[
						SNew(STextBlock).Text(FText::FromString("Flip Orientation"))
					]
			]
		]
		// Mapping Asset picker
		+ SVerticalBox::Slot().AutoHeight().Padding(5.f, 5.f, 5.f, 10.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(5)
			[
				SNew(STextBlock)
					.Text(FText::FromString("Mapping Asset:"))
					.Font(*_fontInfoPtr)
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(5)
			[
				SNew(SObjectPropertyEntryBox)
					.AllowedClass(USignalTypeMapping::StaticClass())
					.ObjectPath(this, &SOpenDRIVEEditorModeWidget::GetSignalMappingAssetPath)
					.OnObjectChanged(this, &SOpenDRIVEEditorModeWidget::OnSignalMappingAssetSelected)
			]
		]
		// --- Assembly Settings ---
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 5.f, 0.f, 0.f).HAlign(HAlign_Center) [ SNew(SSeparator) ]
		// Enable Assembly checkbox
		+ SVerticalBox::Slot().AutoHeight().Padding(5.f, 5.f, 0.f, 0.f)
		[
			SAssignNew(_enableAssemblyCheckBox, SCheckBox)
				.IsChecked(ECheckBoxState::Unchecked)
				.OnCheckStateChanged(this, &SOpenDRIVEEditorModeWidget::OnEnableAssemblyCheckStateChanged)
				.Content()
				[
					SNew(STextBlock).Text(FText::FromString("Enable Assembly"))
				]
		]
		// Distance Threshold slider
		+ SVerticalBox::Slot().AutoHeight().Padding(5.f, 5.f, 5.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(5)
			[
				SNew(STextBlock)
					.Text(FText::FromString("Distance (m):"))
					.Font(*_fontInfoPtr)
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(5)
			[
				SNew(SSpinBox<float>)
					.MinValue(0.1f)
					.MaxValue(50.0f)
					.Value(5.0f)
					.OnValueChanged(this, &SOpenDRIVEEditorModeWidget::OnAssemblyDistanceThresholdChanged)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(5, 0, 0, 0)
			[
				SAssignNew(_assemblyDistanceTextPtr, STextBlock)
					.Text(FText::FromString("5.0"))
					.Font(*_fontInfoPtr)
			]
		]
		// Heading Tolerance slider
		+ SVerticalBox::Slot().AutoHeight().Padding(5.f, 5.f, 5.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(5)
			[
				SNew(STextBlock)
					.Text(FText::FromString("Heading Tol (\xC2\xB0):"))
					.Font(*_fontInfoPtr)
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(5)
			[
				SNew(SSpinBox<float>)
					.MinValue(1.0f)
					.MaxValue(90.0f)
					.Value(15.0f)
					.OnValueChanged(this, &SOpenDRIVEEditorModeWidget::OnAssemblyHeadingToleranceChanged)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(5, 0, 0, 0)
			[
				SAssignNew(_assemblyHeadingTextPtr, STextBlock)
					.Text(FText::FromString("15.0"))
					.Font(*_fontInfoPtr)
			]
		]
		// Assembly Mapping Asset picker
		+ SVerticalBox::Slot().AutoHeight().Padding(5.f, 5.f, 5.f, 10.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(5)
			[
				SNew(STextBlock)
					.Text(FText::FromString("Assembly Mapping:"))
					.Font(*_fontInfoPtr)
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(5)
			[
				SNew(SObjectPropertyEntryBox)
					.AllowedClass(USignalAssemblyMapping::StaticClass())
					.ObjectPath(this, &SOpenDRIVEEditorModeWidget::GetAssemblyMappingAssetPath)
					.OnObjectChanged(this, &SOpenDRIVEEditorModeWidget::OnAssemblyMappingAssetSelected)
			]
		];
}

FOpenDRIVEEditorMode* SOpenDRIVEEditorModeWidget::GetEdMode() const
{
	return (FOpenDRIVEEditorMode*)GLevelEditorModeTools().GetActiveMode(FOpenDRIVEEditorMode::EM_RoadMode);
}

FReply SOpenDRIVEEditorModeWidget::Reset()
{
	GetEdMode()->ResetRoadsArray();
	_showArrowsCheckBox.Get()->SetIsChecked(ECheckBoxState::Unchecked);
	return FReply::Handled();
}

bool SOpenDRIVEEditorModeWidget::IsLoaded() const
{
	return (GetEdMode()->GetHasBeenLoaded() && CheckIfInEditorMode());
}

bool SOpenDRIVEEditorModeWidget::CheckIfInEditorMode() const
{
	return !(GEditor->IsPlayingSessionInEditor());
}

FReply SOpenDRIVEEditorModeWidget::Generate()
{
	GetEdMode()->Generate();
	_showArrowsCheckBox.Get()->SetIsChecked(ECheckBoxState::Unchecked);
	return FReply::Handled();
}

void SOpenDRIVEEditorModeWidget::UpdateLaneInfo(AOpenDriveEditorLane* lane_)
{
	_roadIdTextPtr.Get()->SetText(FText::FromString("Road Id : " + FString::FromInt(lane_->GetRoadId())));

	_junctionIdTextPtr.Get()->SetText(FText::FromString("Junction Id : " + FString::FromInt(lane_->GetJunctionId())));

	_laneTypeTextPtr.Get()->SetText(FText::FromString("Lane type : " + lane_->GetLaneType()));

	_laneIdTextPtr.Get()->SetText(FText::FromString("Lane Id : " + FString::FromInt(lane_->GetLaneId())));

	_successorIdTextPtr.Get()->SetText(FText::FromString("Successor Id : " + FString::FromInt(lane_->GetSuccessorId())));

	_predecessorIdTextPtr.Get()->SetText(FText::FromString("Predecessor Id : " + FString::FromInt(lane_->GetPredecessorId())));
}

void SOpenDRIVEEditorModeWidget::SetOffset(const FText &newOffset_)
{
	FString string = newOffset_.ToString();

	float offset = string.IsNumeric() ? FCString::Atof(*string) : 10.f;

	GetEdMode()->SetRoadOffset(offset);
}

void SOpenDRIVEEditorModeWidget::OnCheckStateChanged(ECheckBoxState state)
{
	GetEdMode()->SetRoadsArrowsVisibilityInEditor(state == ECheckBoxState::Checked);
}

void SOpenDRIVEEditorModeWidget::OnOffsetValueChanged(float value)
{
	_offsetTextPtr->SetText(FText::FromString("Road's Zoffset : " + FString::FormatAsNumber(value)));
	GetEdMode()->SetRoadOffset(value);
}

void SOpenDRIVEEditorModeWidget::OnStepValueChanged(float value)
{
	_stepTextPtr->SetText(FText::FromString("Step : " + FString::FormatAsNumber(value)));
	GetEdMode()->SetStep(value);
}

FReply SOpenDRIVEEditorModeWidget::GenerateLaneSplines()
{
	GetEdMode()->GenerateLaneSplines();
	return FReply::Handled();
}

FReply SOpenDRIVEEditorModeWidget::ClearGeneratedSplines()
{
	GetEdMode()->ClearGeneratedSplines();
	return FReply::Handled();
}

void SOpenDRIVEEditorModeWidget::SetAllLaneTypeCheckBoxes(bool bChecked)
{
	ECheckBoxState State = bChecked ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;

	TSharedPtr<SCheckBox> CheckBoxes[] = {
		_drivingCheckBox, _sidewalkCheckBox, _bikingCheckBox,
		_parkingCheckBox, _shoulderCheckBox, _restrictedCheckBox,
		_medianCheckBox, _otherCheckBox, _referenceCheckBox
	};

	for (auto& CB : CheckBoxes)
	{
		if (CB.IsValid())
		{
			CB->SetIsChecked(State);
		}
	}

	FOpenDRIVEEditorMode* EdMode = GetEdMode();
	EdMode->SetGenerateDrivingLane(bChecked);
	EdMode->SetGenerateSidewalkLane(bChecked);
	EdMode->SetGenerateBikingLane(bChecked);
	EdMode->SetGenerateParkingLane(bChecked);
	EdMode->SetGenerateShoulderLane(bChecked);
	EdMode->SetGenerateRestrictedLane(bChecked);
	EdMode->SetGenerateMedianLane(bChecked);
	EdMode->SetGenerateOtherLane(bChecked);
	EdMode->SetGenerateReferenceLane(bChecked);
}

void SOpenDRIVEEditorModeWidget::OnDrivingLaneCheckStateChanged(ECheckBoxState state)
{
	GetEdMode()->SetGenerateDrivingLane(state == ECheckBoxState::Checked);
}

void SOpenDRIVEEditorModeWidget::OnSidewalkLaneCheckStateChanged(ECheckBoxState state)
{
	GetEdMode()->SetGenerateSidewalkLane(state == ECheckBoxState::Checked);
}

void SOpenDRIVEEditorModeWidget::OnGenerateRoadsCheckStateChanged(ECheckBoxState state)
{
	GetEdMode()->SetGenerateRoads(state == ECheckBoxState::Checked);
}

void SOpenDRIVEEditorModeWidget::OnGenerateJunctionsCheckStateChanged(ECheckBoxState state)
{
	GetEdMode()->SetGenerateJunctions(state == ECheckBoxState::Checked);
}

void SOpenDRIVEEditorModeWidget::OnBikingLaneCheckStateChanged(ECheckBoxState state)
{
	GetEdMode()->SetGenerateBikingLane(state == ECheckBoxState::Checked);
}

void SOpenDRIVEEditorModeWidget::OnParkingLaneCheckStateChanged(ECheckBoxState state)
{
	GetEdMode()->SetGenerateParkingLane(state == ECheckBoxState::Checked);
}

void SOpenDRIVEEditorModeWidget::OnShoulderLaneCheckStateChanged(ECheckBoxState state)
{
	GetEdMode()->SetGenerateShoulderLane(state == ECheckBoxState::Checked);
}

void SOpenDRIVEEditorModeWidget::OnRestrictedLaneCheckStateChanged(ECheckBoxState state)
{
	GetEdMode()->SetGenerateRestrictedLane(state == ECheckBoxState::Checked);
}

void SOpenDRIVEEditorModeWidget::OnMedianLaneCheckStateChanged(ECheckBoxState state)
{
	GetEdMode()->SetGenerateMedianLane(state == ECheckBoxState::Checked);
}

void SOpenDRIVEEditorModeWidget::OnOtherLaneCheckStateChanged(ECheckBoxState state)
{
	GetEdMode()->SetGenerateOtherLane(state == ECheckBoxState::Checked);
}

void SOpenDRIVEEditorModeWidget::OnReferenceLaneCheckStateChanged(ECheckBoxState state)
{
	GetEdMode()->SetGenerateReferenceLane(state == ECheckBoxState::Checked);
}

void SOpenDRIVEEditorModeWidget::OnLeftLanesCheckStateChanged(ECheckBoxState state)
{
	GetEdMode()->SetGenerateLeftLanes(state == ECheckBoxState::Checked);
}

void SOpenDRIVEEditorModeWidget::OnRightLanesCheckStateChanged(ECheckBoxState state)
{
	GetEdMode()->SetGenerateRightLanes(state == ECheckBoxState::Checked);
}

TSharedRef<SWidget> SOpenDRIVEEditorModeWidget::MakeLanePositionFilterWidget(TSharedPtr<FString> InOption)
{
	return SNew(STextBlock).Text(FText::FromString(*InOption)).Font(*_fontInfoPtr);
}

void SOpenDRIVEEditorModeWidget::OnLanePositionFilterChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type Type)
{
	if (!NewValue.IsValid()) return;

	FSplineGenerator::ELanePositionFilter Filter = FSplineGenerator::ELanePositionFilter::All;
	if (*NewValue == "Outermost Only") Filter = FSplineGenerator::ELanePositionFilter::OutermostOnly;
	else if (*NewValue == "Outermost Driving Only") Filter = FSplineGenerator::ELanePositionFilter::OutermostDrivingOnly;
	else if (*NewValue == "Innermost Only") Filter = FSplineGenerator::ELanePositionFilter::InnermostOnly;
	else if (*NewValue == "Innermost Driving Only") Filter = FSplineGenerator::ELanePositionFilter::InnermostDrivingOnly;
	else if (*NewValue == "Specific Index") Filter = FSplineGenerator::ELanePositionFilter::SpecificIndex;

	GetEdMode()->SetLanePositionFilter(Filter);
}

void SOpenDRIVEEditorModeWidget::OnSpecificLaneIndexChanged(int32 NewValue, ETextCommit::Type CommitType)
{
	GetEdMode()->SetSpecificLaneIndex(NewValue);
}

TSharedRef<SWidget> SOpenDRIVEEditorModeWidget::MakeSplineResampleModeWidget(TSharedPtr<FString> InOption)
{
	return SNew(STextBlock).Text(FText::FromString(*InOption)).Font(*_fontInfoPtr);
}

void SOpenDRIVEEditorModeWidget::OnSplineResampleModeChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type Type)
{
	if (!NewValue.IsValid()) return;

	if (*NewValue == "Center")
	{
		GetEdMode()->SetSplineGenerationMode(AOpenDriveLaneSpline::Center);
	}
	else if (*NewValue == "Inside")
	{
		GetEdMode()->SetSplineGenerationMode(AOpenDriveLaneSpline::Inside);
	}
	else if (*NewValue == "Outside")
	{
		GetEdMode()->SetSplineGenerationMode(AOpenDriveLaneSpline::Outside);
	}
}

FReply SOpenDRIVEEditorModeWidget::GenerateSignals()
{
	GetEdMode()->GenerateSignals();
	return FReply::Handled();
}

void SOpenDRIVEEditorModeWidget::OnGenerateSignalsCheckStateChanged(ECheckBoxState state)
{
	GetEdMode()->SignalGenerator.SetGenerateSignals(state == ECheckBoxState::Checked);
}

void SOpenDRIVEEditorModeWidget::OnFlipSignalOrientationCheckStateChanged(ECheckBoxState state)
{
	GetEdMode()->SignalGenerator.SetFlipSignalOrientation(state == ECheckBoxState::Checked);
}

FString SOpenDRIVEEditorModeWidget::GetSignalMappingAssetPath() const
{
	USignalTypeMapping* Asset = GetEdMode()->SignalGenerator.GetSignalTypeMappingAsset();
	return Asset ? Asset->GetPathName() : FString();
}

void SOpenDRIVEEditorModeWidget::OnSignalMappingAssetSelected(const FAssetData& AssetData)
{
	USignalTypeMapping* Asset = Cast<USignalTypeMapping>(AssetData.GetAsset());
	GetEdMode()->SignalGenerator.SetSignalTypeMappingAsset(Asset);
}

// === Signal Assembly callbacks ===

void SOpenDRIVEEditorModeWidget::OnEnableAssemblyCheckStateChanged(ECheckBoxState state)
{
	GetEdMode()->SignalGenerator.SetEnableAssembly(state == ECheckBoxState::Checked);
}

void SOpenDRIVEEditorModeWidget::OnAssemblyDistanceThresholdChanged(float value)
{
	GetEdMode()->SignalGenerator.SetAssemblyDistanceThreshold(value);
	if (_assemblyDistanceTextPtr.IsValid())
	{
		_assemblyDistanceTextPtr->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), value)));
	}
}

void SOpenDRIVEEditorModeWidget::OnAssemblyHeadingToleranceChanged(float value)
{
	GetEdMode()->SignalGenerator.SetAssemblyHeadingTolerance(value);
	if (_assemblyHeadingTextPtr.IsValid())
	{
		_assemblyHeadingTextPtr->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), value)));
	}
}

FString SOpenDRIVEEditorModeWidget::GetAssemblyMappingAssetPath() const
{
	USignalAssemblyMapping* Asset = GetEdMode()->SignalGenerator.GetSignalAssemblyMappingAsset();
	return Asset ? Asset->GetPathName() : FString();
}

void SOpenDRIVEEditorModeWidget::OnAssemblyMappingAssetSelected(const FAssetData& AssetData)
{
	USignalAssemblyMapping* Asset = Cast<USignalAssemblyMapping>(AssetData.GetAsset());
	GetEdMode()->SignalGenerator.SetSignalAssemblyMappingAsset(Asset);
}

