from xml.sax.saxutils import escape
import codecs

class MarkdownTablesOutput():
    def __init__(self, groups, board):
        result = ("# Airframes Reference\n"
                  "> **Note** **This list is auto-generated from the source code** and contains the most recent airframes documentation.\n"
                  "> \n"
                  "> The **AUX** channels are only available on Pixhawk Boards (labeled with **AUX OUT**).\n"
                  "\n")

        # TODO: describe meaning of green + blue color...

        for group in groups:

            result += '## %s\n\n' % group.GetName()

            # Display an image of the frame
            image_name = group.GetImageName()
            result += '<div>\n'
            if image_name != 'AirframeUnknown':
                result += '<img src="images/airframes/types/%s.svg" width="25%%" style="max-height: 150px;"/>\n' % (image_name)

            # check if all outputs are equal for the group: if so, show them
            # only once
            outputs_prev = ['', ''] # split into MAINx and others (AUXx)
            outputs_match = [True, True]
            for param in group.GetParams():
                if not self.IsExcluded(param, board):
                    outputs_current = ['', '']
                    for output_name in param.GetOutputCodes():
                        value = param.GetOutputValue(output_name)
                        if output_name.lower().startswith('main'):
                            idx = 0
                        else:
                            idx = 1
                        outputs_current[idx] += '<li><b>%s</b>: %s</li>' % (output_name, value)
                    for i in range(2):
                        if len(outputs_current[i]) != 0:
                            if outputs_prev[i] == '':
                                outputs_prev[i] = outputs_current[i]
                            elif outputs_current[i] != outputs_prev[i]:
                                outputs_match[i] = False

            for i in range(2):
                if len(outputs_prev[i]) == 0:
                    outputs_match[i] = False
                if not outputs_match[i]:
                    outputs_prev[i] = ''

            if outputs_match[0] or outputs_match[1]:
                result += '<table style="float: right; width: 70%; font-size:1.5rem;">\n'
                result += ' <colgroup><col></colgroup>\n'
                result += ' <thead>\n'
                result += '   <tr><th>Common Outputs</th></tr>\n'
                result += ' </thead>\n'
                result += '<tbody>\n'
                result += '<tr>\n <td style="vertical-align: top;"><ul>%s%s</ul></td>\n</tr>\n' % (outputs_prev[0], outputs_prev[1])
                result += '</tbody></table>\n'

            result += '</div>\n\n'

            result += '<table style="width: 100%; table-layout:fixed; font-size:1.5rem;">\n'
            result += ' <colgroup><col style="width: 30%"><col style="width: 30%"><col style="width: 40%"></colgroup>\n'
            result += ' <thead>\n'
            result += '   <tr><th>Name</th><th></th><th>Specific Outputs</th></tr>\n'
            result += ' </thead>\n'
            result += '<tbody>\n'

            for param in group.GetParams():
                if not self.IsExcluded(param, board):
                    #print("generating: {0} {1}".format(param.GetName(), excluded))
                    name = param.GetName()
                    airframe_id = param.GetId()
                    airframe_id_entry = '<p><code>SYS_AUTOSTART</code> = %s</p>' % (airframe_id)
                    maintainer = param.GetMaintainer()
                    maintainer_entry = ''
                    if maintainer != '':
                        maintainer_entry = '<p>Maintainer: %s</p>' % (maintainer)
                    url = param.GetFieldValue('url')
                    name_entry = name
                    if url != '':
                        name_entry = '<a href="%s">%s</a>' % (url, name)
                    outputs = '<ul>'
                    for output_name in param.GetOutputCodes():
                        value = param.GetOutputValue(output_name)
                        valstrs = value.split(";")
                        if output_name.lower().startswith('main'):
                            idx = 0
                        else:
                            idx = 1
                        if not outputs_match[idx]:
                            outputs += '<li><b>%s</b>: %s</li>' % (output_name, value)

                        for attrib in valstrs[1:]:
                            attribstrs = attrib.split(":")
                            # some airframes provide more info, like angle=60, direction=CCW
                            #print(output_name,value, attribstrs[0].strip(),attribstrs[1].strip())
                    outputs += '</ul>'

                    result += '<tr>\n <td style="vertical-align: top;">%s</td>\n <td style="vertical-align: top;">%s%s</td>\n <td style="vertical-align: top;">%s</td>\n</tr>\n' % (name_entry, maintainer_entry, airframe_id_entry, outputs)


            #Close the table.
            result += '</tbody></table>\n\n'

        self.output = result

    def IsExcluded(self, param, board):
        for code in param.GetArchCodes():
            if "CONFIG_ARCH_BOARD_{0}".format(code) == board and param.GetArchValue(code) == "exclude":
                return True
        return False

    def Save(self, filename):
        with codecs.open(filename, 'w', 'utf-8') as f:
            f.write(self.output)
