from PyQt4 import QtGui, QtCore
from widgets.combochoice import ComboChoice
from widgets.stringbox import DoubleBox
from widgets.pathchooser import PathChooser
from pages.config.parameters.parametermodels import DataModel
import enums
import widgets.helpedwidget

class DataPanel(QtGui.QFrame):

    def __init__(self, parent):
        QtGui.QFrame.__init__(self, parent)

        self.setFrameShape(QtGui.QFrame.StyledPanel)
        self.setFrameShadow(QtGui.QFrame.Plain)
        self.setSizePolicy(QtGui.QSizePolicy.Expanding, QtGui.QSizePolicy.Expanding)

        layout = QtGui.QFormLayout()
        layout.setLabelAlignment(QtCore.Qt.AlignRight)

        self.dataModel = DataModel("")

        self.input = ComboChoice(self, enums.gen_data_file_format.INPUT_TYPES, "", "config/ensemble/gen_data_param_init")
        self.modelWrap(self.input, "input_format")

        self.output = ComboChoice(self, enums.gen_data_file_format.OUTPUT_TYPES, "", "config/ensemble/gen_data_param_output")
        self.modelWrap(self.output, "output_format")

        self.template_file = PathChooser(self, "", "config/ensemble/gen_data_template_file", True , must_be_set=False)
        self.modelWrap(self.template_file, "template_file")

        self.template_key = PathChooser(self, "", "config/ensemble/gen_data_template_key", True , must_be_set=False)
        self.modelWrap(self.template_key, "template_key")

        self.init_file_fmt = PathChooser(self, "", "config/ensemble/gen_data_init_file_fmt", True , must_be_set=False)
        self.modelWrap(self.init_file_fmt, "init_file_fmt")


        self.file_generated_by_enkf = PathChooser(self, "", "config/ensemble/gen_data_file_generated_by_enkf", True, must_be_set=False)
        self.modelWrap(self.file_generated_by_enkf, "enkf_outfile")

        self.file_loaded_by_enkf = PathChooser(self, "", "config/ensemble/gen_data_file_loaded_by_enkf", True, must_be_set=False)
        self.modelWrap(self.file_loaded_by_enkf, "enkf_infile")

        self.min_std = PathChooser(self, "", "config/ensemble/gen_data_min_std", True, must_be_set=False)
        self.modelWrap(self.min_std, "min_std")

        layout.addRow("Input:", self.input)
        layout.addRow("Output:", self.output)
        layout.addRow("Template file:", self.template_file)
        layout.addRow("Template key:", self.template_key)
        layout.addRow("Init files:", self.init_file_fmt)
        layout.addRow("Include file:", self.file_generated_by_enkf)
        layout.addRow("Min. std.:", self.min_std)
        layout.addRow("File loaded by EnKF:", self.file_loaded_by_enkf)

        button = QtGui.QPushButton()
        button.setText("Reload")
        button.setMaximumWidth(70)
        self.connect(button, QtCore.SIGNAL('clicked()'), self.dataModel.emitUpdate)

        layout.addRow("Reload template:", button)

        self.setLayout(layout)


    def modelWrap(self, widget, attribute):
        widget.initialize = widgets.helpedwidget.ContentModel.emptyInitializer
        widget.setter = lambda model, value: self.dataModel.set(attribute, value)
        widget.getter = lambda model: self.dataModel[attribute]

    def setDataModel(self, dataModel):
        self.dataModel = dataModel

        self.input.fetchContent()
        self.output.fetchContent()
        self.template_file.fetchContent()
        self.template_key.fetchContent()
        self.init_file_fmt.fetchContent()
        self.file_generated_by_enkf.fetchContent()
        self.file_loaded_by_enkf.fetchContent()
        self.min_std.fetchContent()