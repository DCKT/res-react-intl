open ReactIntl

module Msg = {
  @@intl.messages
  let hello = {defaultMessage: "Hello rescript"}
  let helloAgain = {defaultMessage: "Hello again rescript", id: "A"}
}

module Component1 = {
  @react.component
  let make = () => <FormattedMessage defaultMessage="Some default message rescript" />
}

module Component2 = {
  @react.component
  let make = () => <FormattedMessage defaultMessage="Some default message with id rescript" id="B" />
}

module Component3 = {
  @react.component
  let make = () =>
    <FormattedMessage
      defaultMessage="Some default message with id and values {foo} rescript"
      id="C"
      values={
        "foo": <FormattedMessage defaultMessage="Somme wrapped message rescript" id="D" />,
      }
    />
}
