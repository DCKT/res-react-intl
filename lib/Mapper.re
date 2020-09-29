open Ast_mapper;
open Asttypes;
open Parsetree;
open Longident;

let idLength = 8;

let generateId = value => {
  "_"
  ++ (value |> Digest.string |> Digest.to_hex |> String.sub(_, 0, idLength));
};

let findByLabelName = (key, labels) =>
  labels
  |> List.find_opt(label =>
       switch (label) {
       | (Labelled(labelName), _) when labelName == key => true
       | _ => false
       }
     );

let getValueFromLabel = label =>
  switch (label) {
  | (_, {pexp_desc: Pexp_constant(Pconst_string(value, _)), _}) => value
  | _ => raise(Not_found)
  };

let replaceIdFromLabels = (~loc, labels) => {
  let id = labels |> findByLabelName("id");
  let defaultMessage = labels |> findByLabelName("defaultMessage");

  switch (id, defaultMessage) {
  | (None, Some(defaultMessage)) =>
    labels
    |> List.append(
         {
           [
             (
               Labelled("id"),
               defaultMessage
               |> getValueFromLabel
               |> generateId
               |> Ast_helper.Const.string
               |> Ast_helper.Exp.constant,
             ),
           ];
         },
       )
  | (Some(_), _) => labels
  | (None, None) =>
    raise(
      Location.Error(
        Location.error(~loc, "Missing id for ReactIntl message"),
      ),
    )
  };
};

let findByFieldName = (key, fields) =>
  fields
  |> List.find_opt(field =>
       switch (field) {
       | ({txt: Lident(fieldName), _}, _) when fieldName == key => true
       | _ => false
       }
     );

let getValueFromField = field =>
  switch (field) {
  | (_, {pexp_desc: Pexp_constant(Pconst_string(value, _)), _}) => value
  | _ => raise(Not_found)
  };

let replaceIdFromRecord = (~loc, fields) => {
  let id = fields |> findByFieldName("id");
  let defaultMessage = fields |> findByFieldName("defaultMessage");

  switch (id, defaultMessage) {
  | (None, Some(defaultMessage)) =>
    fields
    |> List.append([
         (
           {txt: Lident("id"), loc: Ast_helper.default_loc.contents},
           defaultMessage
           |> getValueFromField
           |> generateId
           |> Ast_helper.Const.string
           |> Ast_helper.Exp.constant,
         ),
       ])
  | (Some(_), _) => fields
  | (None, None) =>
    raise(
      Location.Error(
        Location.error(~loc, "Missing id for ReactIntl message"),
      ),
    )
  };
};

let extractMessageFromLabels = (labels, callback) => {
  let map =
    labels
    |> List.fold_left(
         (map, label) =>
           switch (label) {
           | (
               Labelled(key),
               {pexp_desc: Pexp_constant(Pconst_string(value, _)), _},
             ) =>
             map |> StringMap.add(key, value)
           | _ => map
           },
         StringMap.empty,
       );

  switch (Message.fromStringMap(map)) {
  | Some(value) => callback(value)
  | None => ()
  };

  labels;
};

let extractMessageFromRecord = (fields, callback) => {
  let map =
    fields
    |> List.fold_left(
         (map, field) =>
           switch (field) {
           | (
               {txt: Lident(key), _},
               {pexp_desc: Pexp_constant(Pconst_string(value, _)), _},
             ) =>
             map |> StringMap.add(key, value)
           | _ => map
           },
         StringMap.empty,
       );

  switch (Message.fromStringMap(map)) {
  | Some(value) => callback(value)
  | None => ()
  };

  fields;
};

let hasIntlAttribute = (items: structure) =>
  items
  |> List.exists(item =>
       switch (item) {
       | {pstr_desc: Pstr_attribute(({txt: "intl.messages", _}, _)), _} =>
         true
       | _ => false
       }
     );

let extractMessagesFromValueBindings =
    (mapper, valueBindings: list(value_binding), callback) =>
  valueBindings
  |> List.map(valueBinding =>
       switch (valueBinding) {
       | {
           pvb_pat: {ppat_desc: Ppat_var(_), _} as pattern,
           pvb_expr: {pexp_desc: Pexp_record(fields, None), _} as expr,
           _,
         } =>
         Ast_helper.Vb.mk(
           ~attrs=valueBinding.pvb_attributes,
           ~loc=valueBinding.pvb_loc,
           pattern,
           Ast_helper.Exp.record(
             ~attrs=expr.pexp_attributes,
             ~loc=expr.pexp_loc,
             extractMessageFromRecord(
               fields |> replaceIdFromRecord(~loc=expr.pexp_loc),
               callback,
             ),
             None,
           ),
         )
       | _ => default_mapper.value_binding(mapper, valueBinding)
       }
     );

let matchesFormattedMessage = ident =>
  switch (ident) {
  | Ldot(Ldot(Lident("ReactIntl"), "FormattedMessage"), "createElement")
  | Ldot(Lident("FormattedMessage"), "createElement") => true
  | _ => false
  };

let getMapper = (callback: Message.t => unit): mapper => {
  ...default_mapper,

  // Match records in modules with [@intl.messages]
  // (structure is the module body - either top level or of a submodule)
  structure: (mapper, structure) =>
    if (hasIntlAttribute(structure)) {
      default_mapper.structure(
        mapper,
        structure
        |> List.map(item =>
             switch (item) {
             | {
                 pstr_desc: Pstr_value(Nonrecursive, valueBindings),
                 pstr_loc: loc,
               } =>
               Ast_helper.Str.value(
                 ~loc,
                 Nonrecursive,
                 extractMessagesFromValueBindings(
                   mapper,
                   valueBindings,
                   callback,
                 ),
               )
             | _ => default_mapper.structure_item(mapper, item)
             }
           ),
      );
    } else {
      default_mapper.structure(mapper, structure);
    },

  expr: (mapper, expr) => {
    switch (expr) {
    // Match (ReactIntl.)FormattedMessage.createElement
    | {
        pexp_desc:
          Pexp_apply(
            {pexp_desc: Pexp_ident({txt, _}), _} as applyExpr,
            labels,
          ),
        _,
      }
        when matchesFormattedMessage(txt) =>
      Ast_helper.Exp.apply(
        ~attrs=expr.pexp_attributes,
        ~loc=expr.pexp_loc,
        applyExpr,
        extractMessageFromLabels(
          labels |> replaceIdFromLabels(~loc=applyExpr.pexp_loc),
          callback,
        ),
      )
    | _ => default_mapper.expr(mapper, expr)
    };
  },
};
