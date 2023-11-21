add_subdirectory(Qt-AES)
add_subdirectory(qtmqtt)

# what kind of black magic is this
set_target_properties(QtAES PROPERTIES DEFINE_SYMBOL "")
set_target_properties(Mqtt PROPERTIES DEFINE_SYMBOL "")
